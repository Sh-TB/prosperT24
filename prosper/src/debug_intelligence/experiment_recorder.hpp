/**
 * Experiment Recorder Module
 * 
 * Captures and persists experiment state including:
 * - Build configuration (compiler, git state, defines)
 * - Environment variables and system info
 * - Runtime logs
 * - Screenshots
 * - Crash state
 * - EXP package generation
 */

#pragma once

#include "debug_intelligence.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace debug_intelligence {

/**
 * Result of a capture operation
 */
struct CaptureResult {
    bool success;
    std::string message;
    std::string captured_path;
    size_t bytes_captured;
    
    static CaptureResult ok(const std::string& path = "", size_t bytes = 0) {
        return {true, "Capture successful", path, bytes};
    }
    
    static CaptureResult error(const std::string& msg) {
        return {false, msg, "", 0};
    }
};

/**
 * Experiment Recorder - Main interface for capturing experiment data
 */
class ExperimentRecorder {
public:
    explicit ExperimentRecorder(const fs::path& base_dir)
        : m_base_dir(base_dir), m_initialized(false) {}
    
    /**
     * Initialize recorder directory structure
     */
    bool initialize() {
        try {
            m_evidence_dir = m_base_dir / EVIDENCE_DIR;
            m_screenshots_dir = m_base_dir / SCREENSHOTS_DIR;
            m_logs_dir = m_base_dir / LOGS_DIR;
            
            fs::create_directories(m_evidence_dir);
            fs::create_directories(m_screenshots_dir);
            fs::create_directories(m_logs_dir);
            
            m_initialized = true;
            return true;
        } catch (const fs::filesystem_error& e) {
            m_last_error = e.what();
            return false;
        }
    }
    
    // ========================================================================
    // Build Configuration Capture
    // ========================================================================
    
    /**
     * Capture current build configuration from environment
     */
    BuildConfiguration captureBuildConfig() const {
        BuildConfiguration config;
        
        // Compiler information
        config.compiler = getEnvOrDefault("CXX", "g++");
        config.compiler_version = captureCompilerVersion();
        
        // Build type
        config.build_type = getEnvOrDefault("CMAKE_BUILD_TYPE", "Debug");
        
        // CMake arguments
        config.cmake_args = getEnvOrDefault("CMAKE_ARGS", "");
        
        // C++ standard
        config.cxx_standard = getEnvOrDefault("CXX_STANDARD", "17");
        
        // Git information
        captureGitInfo(config);
        
        // Compiler defines (from common build systems)
        captureCompilerDefines(config);
        
        return config;
    }
    
    /**
     * Parse build configuration from CMake cache file
     */
    std::optional<BuildConfiguration> parseFromCmakeCache(const fs::path& cmake_cache_path) const {
        if (!fs::exists(cmake_cache_path)) {
            return std::nullopt;
        }
        
        BuildConfiguration config;
        std::ifstream file(cmake_cache_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // CMake cache format: NAME:TYPE=value
            auto colon_pos = line.find(':');
            if (colon_pos == std::string::npos) continue;
            
            auto eq_pos = line.find('=', colon_pos);
            if (eq_pos == std::string::npos) continue;
            
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(eq_pos + 1);
            
            if (name == "CMAKE_BUILD_TYPE") {
                config.build_type = value;
            } else if (name == "CMAKE_CXX_COMPILER") {
                config.compiler = value;
            } else if (name == "CMAKE_CXX_STANDARD") {
                config.cxx_standard = value;
            }
        }
        
        // Also capture git info for this source tree
        auto parent_dir = cmake_cache_path.parent_path();
        captureGitInfo(config, parent_dir);
        
        return config;
    }
    
    // ========================================================================
    // Environment Capture
    // ========================================================================
    
    /**
     * Capture current environment snapshot
     */
    EnvironmentSnapshot captureEnvironment() const {
        EnvironmentSnapshot env;
        
        // Capture all environment variables (filtering sensitive ones)
        extern char** environ;
        if (environ) {
            for (char** env_var = environ; *env_var; ++env_var) {
                std::string entry(*env_var);
                auto eq_pos = entry.find('=');
                if (eq_pos != std::string::npos) {
                    std::string name = entry.substr(0, eq_pos);
                    std::string value = entry.substr(eq_pos + 1);
                    
                    // Skip sensitive variables but record they exist
                    if (isSensitive(name)) {
                        env.variables[name] = "[REDACTED]";
                    } else {
                        env.variables[name] = value;
                    }
                }
            }
        }
        
        // System information
        env.os_name = captureOsName();
        env.architecture = captureArchitecture();
        env.hostname = captureHostname();
        env.cpu_count = std::thread::hardware_concurrency();
        env.total_memory_mb = captureTotalMemoryMB();
        
        return env;
    }
    
    /**
     * Capture specific environment variable as evidence
     */
    Evidence captureEnvironmentVar(const std::string& var_name) const {
        Evidence evd;
        evd.type = EvidenceType::EnvironmentVariable;
        evd.description = "Environment variable: " + var_name;
        
        const char* value = std::getenv(var_name.c_str());
        if (value) {
            if (isSensitive(var_name)) {
                evd.content = "[REDACTED]";
                evd.tags["sensitive"] = "true";
            } else {
                evd.content = value;
            }
            evd.severity = Severity::Info;
        } else {
            evd.content = "[NOT SET]";
            evd.severity = Severity::Warning;
        }
        
        evd.source_path = var_name;
        evd.tags["variable_name"] = var_name;
        
        return evd;
    }
    
    // ========================================================================
    // Log Capture
    // ========================================================================
    
    /**
     * Copy log file into evidence store
     */
    CaptureResult captureLogFile(const fs::path& log_path, const std::string& description = "") const {
        if (!m_initialized) {
            return CaptureResult::error("Recorder not initialized");
        }
        
        if (!fs::exists(log_path)) {
            return CaptureResult::error("Log file does not exist: " + log_path.string());
        }
        
        try {
            fs::path dest = m_logs_dir / log_path.filename();
            
            // If file exists, add timestamp to avoid overwriting
            if (fs::exists(dest)) {
                dest = m_logs_dir / (log_path.stem().string() + "_" + Timestamp::fileFriendly() + log_path.extension().string());
            }
            
            fs::copy_file(log_path, dest);
            
            auto file_size = fs::file_size(dest);
            return CaptureResult::ok(dest.string(), file_size);
        } catch (const fs::filesystem_error& e) {
            return CaptureResult::error(e.what());
        }
    }
    
    /**
     * Create evidence from log content string
     */
    Evidence createLogEvidence(const std::string& content, const std::string& description, Severity severity = Severity::Info) const {
        Evidence evd;
        evd.type = EvidenceType::LogEntry;
        evd.description = description.empty() ? "Runtime log entry" : description;
        evd.content = content;
        evd.severity = severity;
        evd.verified = true;
        
        // Auto-detect severity from content
        if (content.find("ERROR") != std::string::npos || 
            content.find("FATAL") != std::string::npos ||
            content.find("CRASH") != std::string::npos) {
            evd.severity = std::max(evd.severity, Severity::Error);
        }
        if (content.find("WARNING") != std::string::npos || 
            content.find("WARN") != std::string::npos) {
            evd.severity = std::max(evd.severity, Severity::Warning);
        }
        
        return evd;
    }
    
    /**
     * Append to experiment log file
     */
    bool appendToExperimentLog(const std::string& message, const std::string& log_name = "experiment.log") const {
        if (!m_initialized) return false;
        
        fs::path log_path = m_logs_dir / log_name;
        try {
            std::ofstream file(log_path, std::ios::app);
            if (file.is_open()) {
                file << "[" << Timestamp::now() << "] " << message << "\n";
                return true;
            }
        } catch (...) {}
        return false;
    }
    
    // ========================================================================
    // Screenshot Capture
    // ========================================================================
    
    /**
     * Register screenshot file in evidence store
     * Note: Actual screenshot capture is platform-specific; this registers an existing screenshot
     */
    CaptureResult registerScreenshot(const fs::path& screenshot_path, const std::string& description = "") const {
        if (!m_initialized) {
            return CaptureResult::error("Recorder not initialized");
        }
        
        if (!fs::exists(screenshot_path)) {
            return CaptureResult::error("Screenshot file does not exist: " + screenshot_path.string());
        }
        
        try {
            fs::path dest = m_screenshots_dir / screenshot_path.filename();
            
            if (fs::exists(dest)) {
                dest = m_screenshots_dir / (screenshot_path.stem().string() + "_" + Timestamp::fileFriendly() + screenshot_path.extension().string());
            }
            
            fs::copy_file(screenshot_path, dest);
            
            auto file_size = fs::file_size(dest);
            return CaptureResult::ok(dest.string(), file_size);
        } catch (const fs::filesystem_error& e) {
            return CaptureResult::error(e.what());
        }
    }
    
    /**
     * Create screenshot evidence entry
     */
    Evidence createScreenshotEvidence(const fs::path& screenshot_path, const std::string& description = "") const {
        Evidence evd;
        evd.type = EvidenceType::Screenshot;
        evd.description = description.empty() ? "Screenshot" : description;
        evd.source_path = screenshot_path.string();
        
        // Store file info as content (actual image stored separately)
        if (fs::exists(screenshot_path)) {
            evd.content = "File: " + screenshot_path.filename().string() + 
                         " Size: " + std::to_string(fs::file_size(screenshot_path)) + " bytes";
            evd.verified = true;
        } else {
            evd.content = "[File not found]";
            evd.verified = false;
            evd.severity = Severity::Warning;
        }
        
        return evd;
    }
    
    // ========================================================================
    // Crash State Capture
    // ========================================================================
    
    /**
     * Create crash state evidence from components
     */
    CrashState createCrashState(
        const std::string& signal_type,
        const std::string& signal_number,
        const std::string& fault_address = "",
        const std::vector<std::string>& stack_trace = {},
        const std::vector<std::string>& registers = {}) const {
        
        CrashState crash;
        crash.signal_type = signal_type;
        crash.signal_number = signal_number;
        crash.fault_address = fault_address;
        crash.stack_trace = stack_trace;
        crash.registers = registers;
        crash.timestamp = Timestamp::now();
        return crash;
    }
    
    /**
     * Convert crash state to evidence
     */
    Evidence createCrashEvidence(const CrashState& crash) const {
        Evidence evd;
        evd.type = EvidenceType::CrashDump;
        evd.description = "Crash: " + crash.signal_type + " (" + crash.signal_number + ")";
        evd.severity = Severity::Critical;
        
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"signal_type\": \"" << crash.signal_type << "\",\n";
        ss << "  \"signal_number\": \"" << crash.signal_number << "\",\n";
        ss << "  \"fault_address\": \"" << crash.fault_address << "\",\n";
        ss << "  \"timestamp\": \"" << crash.timestamp << "\",\n";
        
        if (!crash.stack_trace.empty()) {
            ss << "  \"stack_trace\": [\n";
            for (size_t i = 0; i < crash.stack_trace.size(); ++i) {
                ss << "    \"" << crash.stack_trace[i] << "\"";
                if (i < crash.stack_trace.size() - 1) ss << ",";
                ss << "\n";
            }
            ss << "  ],\n";
        }
        
        if (!crash.registers.empty()) {
            ss << "  \"registers\": {\n";
            for (size_t i = 0; i < crash.registers.size(); ++i) {
                ss << "    " << crash.registers[i];
                if (i < crash.registers.size() - 1) ss << ",";
                ss << "\n";
            }
            ss << "  }\n";
        }
        
        ss << "}";
        evd.content = ss.str();
        evd.verified = true;
        
        return evd;
    }
    
    /**
     * Parse crash log file into CrashState
     */
    std::optional<CrashState> parseCrashLog(const fs::path& crash_log_path) const {
        if (!fs::exists(crash_log_path)) {
            return std::nullopt;
        }
        
        std::ifstream file(crash_log_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        CrashState crash;
        crash.crash_log_path = crash_log_path.string();
        
        std::string line;
        bool in_stack_trace = false;
        bool in_registers = false;
        
        while (std::getline(file, line)) {
            // Try to detect signal info
            if (line.find("Signal") != std::string::npos || line.find("SIGSEGV") != std::string::npos ||
                line.find("SIGABRT") != std::string::npos || line.find("SIGBUS") != std::string::npos) {
                crash.signal_type = line;
            }
            
            // Detect fault address
            if (line.find("fault") != std::string::npos || line.find("address") != std::string::npos) {
                crash.fault_address = line;
            }
            
            // Detect stack trace start
            if (line.find("Stack trace") != std::string::npos || 
                line.find("#0") != std::string::npos ||
                line.find("Backtrace") != std::string::npos) {
                in_stack_trace = true;
                in_registers = false;
            }
            
            // Detect register section
            if (line.find("register") != std::string::npos && !in_stack_trace) {
                in_registers = true;
            }
            
            if (in_stack_trace && !line.empty()) {
                crash.stack_trace.push_back(line);
            }
            
            if (in_registers && !line.empty() && line.find(':') != std::string::npos) {
                crash.registers.push_back(line);
            }
        }
        
        if (crash.signal_type.empty()) {
            return std::nullopt;  // Not a valid crash log
        }
        
        return crash;
    }
    
    // ========================================================================
    // EXP Package Generation
    // ========================================================================
    
    /**
     * Generate complete EXP package from experiment record
     * Returns path to generated package
     */
    std::optional<fs::path> generateExpPackage(const ExperimentRecord& record) const {
        if (!m_initialized) {
            return std::nullopt;
        }
        
        try {
            std::string package_content = serializeExperimentToJson(record);
            
            fs::path package_path = m_base_dir / (record.id + EXP_PACKAGE_EXT);
            
            std::ofstream file(package_path);
            if (!file.is_open()) {
                return std::nullopt;
            }
            
            file << package_content;
            file.close();
            
            return package_path;
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * Load existing EXP package
     */
    std::optional<ExperimentRecord> loadExpPackage(const fs::path& package_path) const {
        if (!fs::exists(package_path) || package_path.extension().string() != EXP_PACKAGE_EXT) {
            return std::nullopt;
        }
        
        std::ifstream file(package_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        return deserializeExperimentFromJson(buffer.str());
    }
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    fs::path getBaseDir() const { return m_base_dir; }
    fs::path getEvidenceDir() const { return m_evidence_dir; }
    fs::path getScreenshotsDir() const { return m_screenshots_dir; }
    fs::path getLogsDir() const { return m_logs_dir; }
    bool isInitialized() const { return m_initialized; }
    std::string getLastError() const { return m_last_error; }
    
private:
    fs::path m_base_dir;
    fs::path m_evidence_dir;
    fs::path m_screenshots_dir;
    fs::path m_logs_dir;
    bool m_initialized;
    mutable std::string m_last_error;
    
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    std::string getEnvOrDefault(const std::string& name, const std::string& default_val) const {
        const char* val = std::getenv(name.c_str());
        return val ? std::string(val) : default_val;
    }
    
    bool isSensitive(const std::string& var_name) const {
        static const std::set<std::string> sensitive_patterns = {
            "PASSWORD", "PASSWD", "SECRET", "KEY", "TOKEN", "API_KEY",
            "API_SECRET", "AUTH", "CREDENTIAL", "PRIVATE"
        };
        
        std::string upper = var_name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        for (const auto& pattern : sensitive_patterns) {
            if (upper.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    std::string captureCompilerVersion() const {
        // Try to get compiler version
        std::string compiler = getEnvOrDefault("CXX", "g++");
        std::string cmd = compiler + " --version";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "unknown";
        
        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        // Just take first line
        auto newline_pos = result.find('\n');
        if (newline_pos != std::string::npos) {
            result = result.substr(0, newline_pos);
        }
        
        return result.empty() ? "unknown" : result;
    }
    
    void captureGitInfo(BuildConfiguration& config, const fs::path& repo_path = fs::current_path()) const {
        auto runGitCommand = [&](const std::string& arg) -> std::string {
            std::string cmd = "git -C " + repo_path.string() + " " + arg + " 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return "";
            
            char buffer[256];
            std::string result;
            if (fgets(buffer, sizeof(buffer), pipe)) {
                result = buffer;
                // Trim newline
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                    result.pop_back();
                }
            }
            pclose(pipe);
            return result;
        };
        
        config.git_commit_hash = runGitCommand("rev-parse HEAD");
        config.git_branch = runGitCommand("rev-parse --abbrev-ref HEAD");
        config.git_commit_message = runGitCommand("log -1 --pretty=%B");
        config.is_dirty = !runGitCommand("status --porcelain").empty();
    }
    
    void captureCompilerDefines(BuildConfiguration& config) const {
        // Common debug/release defines based on build type
        if (config.build_type == "Debug") {
            config.defines.push_back("_DEBUG");
            config.defines.push_back("DEBUG");
        } else if (config.build_type == "Release") {
            config.defines.push_back("NDEBUG");
        }
        
        // Platform-specific
#ifdef _WIN32
        config.defines.push_back("_WIN32");
#elif __linux__
        config.defines.push_back("__linux__");
#endif
    }
    
    std::string captureOsName() const {
#ifdef _WIN32
        return "Windows";
#elif __APPLE__
        return "macOS";
#elif __linux__
        // Try to read from /etc/os-release
        std::ifstream file("/etc/os-release");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("PRETTY_NAME=") == 0) {
                    auto start = line.find('"') + 1;
                    auto end = line.rfind('"');
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        return line.substr(start, end - start);
                    }
                }
            }
        }
        return "Linux";
#else
        return "Unknown";
#endif
    }
    
    std::string captureArchitecture() const {
#if defined(__x86_64__) || defined(_M_X64)
        return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        return "ARM64";
#elif defined(__i386__) || defined(_M_IX86)
        return "x86";
#elif defined(__arm__) || defined(_M_ARM)
        return "ARM";
#else
        return "Unknown";
#endif
    }
    
    std::string captureHostname() const {
        char buffer[256];
        if (gethostname(buffer, sizeof(buffer)) == 0) {
            return std::string(buffer);
        }
        return "unknown";
    }
    
    size_t captureTotalMemoryMB() const {
#if __linux__
        struct sysinfo mem_info;
        if (sysinfo(&mem_info) == 0) {
            return mem_info.totalram / 1024 / 1024;
        }
#endif
        return 0;
    }
    
    // ========================================================================
    // JSON Serialization (Simple implementation without external library)
    // ========================================================================
    
    std::string escapeJson(const std::string& input) const {
        std::string output;
        output.reserve(input.length() * 2);
        for (char c : input) {
            switch (c) {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        output += buf;
                    } else {
                        output += c;
                    }
            }
        }
        return output;
    }
    
    std::string serializeExperimentToJson(const ExperimentRecord& record) const {
        std::stringstream json;
        json << "{\n";
        
        // Basic info
        json << "  \"id\": \"" << escapeJson(record.id) << "\",\n";
        json << "  \"title\": \"" << escapeJson(record.title) << "\",\n";
        json << "  \"description\": \"" << escapeJson(record.description) << "\",\n";
        json << "  \"status\": \"" << escapeJson(record.status) << "\",\n";
        json << "  \"created_at\": \"" << escapeJson(record.created_at) << "\",\n";
        json << "  \"updated_at\": \"" << escapeJson(record.updated_at) << "\",\n";
        json << "  \"completed_at\": \"" << escapeJson(record.completed_at) << "\",\n";
        json << "  \"issue_reference\": \"" << escapeJson(record.issue_reference) << "\",\n";
        
        // Build configuration
        json << "  \"build_config\": {\n";
        auto build_map = record.build_config.toMap();
        for (auto it = build_map.begin(); it != build_map.end(); ++it) {
            json << "    \"" << escapeJson(it->first) << "\": \"" << escapeJson(it->second) << "\"";
            if (std::next(it) != build_map.end()) json << ",";
            json << "\n";
        }
        json << "  },\n";
        
        // Environment (selective - skip full dump for privacy)
        json << "  \"environment\": {\n";
        json << "    \"os_name\": \"" << escapeJson(record.environment.os_name) << "\",\n";
        json << "    \"architecture\": \"" << escapeJson(record.environment.architecture) << "\",\n";
        json << "    \"hostname\": \"" << escapeJson(record.environment.hostname) << "\",\n";
        json << "    \"cpu_count\": " << record.environment.cpu_count << ",\n";
        json << "    \"total_memory_mb\": " << record.environment.total_memory_mb << ",\n";
        json << "    \"timestamp\": \"" << escapeJson(record.environment.timestamp) << "\"\n";
        json << "  },\n";
        
        // Evidence count
        json << "  \"evidence_count\": " << record.evidences.size() << ",\n";
        
        // Evidence list (summary)
        json << "  \"evidences\": [\n";
        auto all_evidence = record.evidences.all();
        for (size_t i = 0; i < all_evidence.size(); ++i) {
            const auto& evd = all_evidence[i];
            json << "    {\n";
            auto evd_map = evd.toMap();
            for (auto it = evd_map.begin(); it != evd_map.end(); ++it) {
                json << "      \"" << escapeJson(it->first) << "\": \"" << escapeJson(it->second) << "\"";
                if (std::next(it) != evd_map.end()) json << ",";
                json << "\n";
            }
            json << "    }";
            if (i < all_evidence.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Hypotheses
        json << "  \"hypotheses\": [\n";
        auto all_hyps = record.hypotheses.all();
        for (size_t i = 0; i < all_hyps.size(); ++i) {
            const auto& hyp = all_hyps[i];
            json << "    {\n";
            json << "      \"id\": \"" << escapeJson(hyp.id) << "\",\n";
            json << "      \"title\": \"" << escapeJson(hyp.title) << "\",\n";
            json << "      \"description\": \"" << escapeJson(hyp.description) << "\",\n";
            json << "      \"status\": \"" << Hypothesis::statusToString(hyp.status) << "\",\n";
            json << "      \"confidence_score\": " << hyp.confidence_score << ",\n";
            json << "      \"supporting_evidence_count\": " << hyp.supporting_evidence_ids.size() << ",\n";
            json << "      \"refuting_evidence_count\": " << hyp.refuting_evidence_ids.size() << "\n";
            json << "    }";
            if (i < all_hyps.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Tags
        json << "  \"tags\": [";
        for (size_t i = 0; i < record.tags.size(); ++i) {
            json << "\"" << escapeJson(record.tags[i]) << "\"";
            if (i < record.tags.size() - 1) json << ", ";
        }
        json << "],\n";
        
        // Metadata
        json << "  \"metadata\": {\n";
        json << "    \"generator_version\": \"" << VERSION << "\",\n";
        json << "    \"generated_at\": \"" << Timestamp::now() << "\"\n";
        json << "  }\n";
        
        json << "}\n";
        return json.str();
    }
    
    std::optional<ExperimentRecord> deserializeExperimentFromJson(const std::string& json_str) const {
        // Simple JSON parser for our specific format
        // In production, use nlohmann/json or similar
        
        ExperimentRecord record;
        
        // Extract basic fields with simple string search
        auto extractString = [&json_str](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            auto pos = json_str.find(search);
            if (pos == std::string::npos) return "";
            
            pos += search.length();
            auto end = json_str.find("\"", pos);
            if (end == std::string::npos) return "";
            
            std::string result = json_str.substr(pos, end - pos);
            // Unescape
            size_t found = 0;
            while ((found = result.find("\\\"", found)) != std::string::npos) {
                result.replace(found, 2, "\"");
            }
            return result;
        };
        
        record.id = extractString("id");
        record.title = extractString("title");
        record.description = extractString("description");
        record.status = extractString("status");
        record.created_at = extractString("created_at");
        record.updated_at = extractString("updated_at");
        record.issue_reference = extractString("issue_reference");
        
        if (record.id.empty()) {
            return std::nullopt;  // Invalid format
        }
        
        return record;
    }
};

} // namespace debug_intelligence
