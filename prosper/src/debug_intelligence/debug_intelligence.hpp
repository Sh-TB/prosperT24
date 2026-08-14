/**
 * Debug Intelligence Layer for Prosper/SharpEmuT24
 * 
 * Evidence Management and Reasoning Assistant for AI-Assisted Debugging
 * 
 * DESIGN PRINCIPLES:
 * - Observer Only: No emulator behavior modifications
 * - Non-Invasive: No loader/HLE/GPU changes
 * - Evidence-Backed: All conclusions require supporting data
 * - AI-Friendly: Structured JSON for LLM consumption
 * 
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <memory>
#include <regex>
#include <system_error>
#include <stdexcept>
#include <array>
#include <ctime>

namespace fs = std::filesystem;

namespace debug_intelligence {

// ============================================================================
// Core Types & Constants
// ============================================================================

constexpr const char* VERSION = "1.0.0";
constexpr const char* EXP_PACKAGE_EXT = ".exp.json";
constexpr const char* EVIDENCE_DIR = "evidence";
constexpr const char* SCREENSHOTS_DIR = "screenshots";
constexpr const char* LOGS_DIR = "logs";

/**
 * Severity levels for diagnostics and evidence
 */
enum class Severity {
    Info,
    Warning,
    Error,
    Critical
};

/**
 * Status tracking for hypotheses and investigations
 */
enum class InvestigationStatus {
    Open,
    InProgress,
    Confirmed,
    Rejected,
    NeedsEvidence,
    Blocked,
    Superseded
};

/**
 * Evidence type classification
 */
enum class EvidenceType {
    LogEntry,
    Screenshot,
    CrashDump,
    MemorySnapshot,
    Configuration,
    EnvironmentVariable,
    BuildInfo,
    GitCommit,
    UserObservation,
    MetricMeasurement,
    CodeDiff,
    NetworkCapture,
    Custom
};

/**
 * Timestamp utility
 */
class Timestamp {
public:
    static std::string now() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
    
    static std::string fileFriendly() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
        return ss.str();
    }
};

// ============================================================================
// Evidence System
// ============================================================================

/**
 * Single piece of evidence with metadata
 */
struct Evidence {
    std::string id;
    EvidenceType type;
    std::string description;
    std::string content;           // Text content or base64 encoded binary
    std::string source_path;       // Original file path (if applicable)
    std::string timestamp;
    Severity severity;
    std::map<std::string, std::string> tags;
    bool verified;
    
    Evidence() : type(EvidenceType::Custom), severity(Severity::Info), verified(false) {
        id = generateId();
        timestamp = Timestamp::now();
    }
    
    static std::string generateId() {
        static uint64_t counter = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "evd_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    // Serialization
    std::map<std::string, std::string> toMap() const {
        return {
            {"id", id},
            {"type", evidenceTypeToString(type)},
            {"description", description},
            {"content", content},
            {"source_path", source_path},
            {"timestamp", timestamp},
            {"severity", severityToString(severity)},
            {"verified", verified ? "true" : "false"}
        };
    }
    
    static EvidenceType stringToEvidenceType(const std::string& str) {
        static const std::map<std::string, EvidenceType> map = {
            {"LogEntry", EvidenceType::LogEntry},
            {"Screenshot", EvidenceType::Screenshot},
            {"CrashDump", EvidenceType::CrashDump},
            {"MemorySnapshot", EvidenceType::MemorySnapshot},
            {"Configuration", EvidenceType::Configuration},
            {"EnvironmentVariable", EvidenceType::EnvironmentVariable},
            {"BuildInfo", EvidenceType::BuildInfo},
            {"GitCommit", EvidenceType::GitCommit},
            {"UserObservation", EvidenceType::UserObservation},
            {"MetricMeasurement", EvidenceType::MetricMeasurement},
            {"CodeDiff", EvidenceType::CodeDiff},
            {"NetworkCapture", EvidenceType::NetworkCapture},
            {"Custom", EvidenceType::Custom}
        };
        auto it = map.find(str);
        return it != map.end() ? it->second : EvidenceType::Custom;
    }
    
    static std::string evidenceTypeToString(EvidenceType type) {
        switch (type) {
            case EvidenceType::LogEntry: return "LogEntry";
            case EvidenceType::Screenshot: return "Screenshot";
            case EvidenceType::CrashDump: return "CrashDump";
            case EvidenceType::MemorySnapshot: return "MemorySnapshot";
            case EvidenceType::Configuration: return "Configuration";
            case EvidenceType::EnvironmentVariable: return "EnvironmentVariable";
            case EvidenceType::BuildInfo: return "BuildInfo";
            case EvidenceType::GitCommit: return "GitCommit";
            case EvidenceType::UserObservation: return "UserObservation";
            case EvidenceType::MetricMeasurement: return "MetricMeasurement";
            case EvidenceType::CodeDiff: return "CodeDiff";
            case EvidenceType::NetworkCapture: return "NetworkCapture";
            case EvidenceType::Custom: return "Custom";
        }
        return "Custom";
    }
    
    static std::string severityToString(Severity sev) {
        switch (sev) {
            case Severity::Info: return "Info";
            case Severity::Warning: return "Warning";
            case Severity::Error: return "Error";
            case Severity::Critical: return "Critical";
        }
        return "Info";
    }
};

/**
 * Evidence collection container
 */
class EvidenceCollection {
public:
    void add(const Evidence& evidence) {
        m_evidences[evidence.id] = evidence;
    }
    
    bool remove(const std::string& id) {
        return m_evidences.erase(id) > 0;
    }
    
    std::optional<Evidence> find(const std::string& id) const {
        auto it = m_evidences.find(id);
        if (it != m_evidences.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<Evidence> findByType(EvidenceType type) const {
        std::vector<Evidence> result;
        for (const auto& [id, evd] : m_evidences) {
            if (evd.type == type) {
                result.push_back(evd);
            }
        }
        return result;
    }
    
    std::vector<Evidence> findByTag(const std::string& tag) const {
        std::vector<Evidence> result;
        for (const auto& [id, evd] : m_evidences) {
            if (evd.tags.count(tag)) {
                result.push_back(evd);
            }
        }
        return result;
    }
    
    std::vector<Evidence> all() const {
        std::vector<Evidence> result;
        for (const auto& [id, evd] : m_evidences) {
            result.push_back(evd);
        }
        return result;
    }
    
    size_t size() const { return m_evidences.size(); }
    bool empty() const { return m_evidences.empty(); }
    
private:
    std::map<std::string, Evidence> m_evidences;
};

// ============================================================================
// Hypothesis System
// ============================================================================

/**
 * A hypothesis about the root cause of an issue
 */
struct Hypothesis {
    std::string id;
    std::string title;
    std::string description;
    InvestigationStatus status;
    std::string created_at;
    std::string updated_at;
    std::string confirmed_at;
    std::string rejected_at;
    std::vector<std::string> supporting_evidence_ids;
    std::vector<std::string> refuting_evidence_ids;
    std::vector<std::string> related_hypothesis_ids;
    double confidence_score;  // 0.0 to 1.0
    std::string notes;
    
    Hypothesis() : status(InvestigationStatus::Open), confidence_score(0.0) {
        id = generateId();
        created_at = Timestamp::now();
        updated_at = created_at;
    }
    
    static std::string generateId() {
        static uint64_t counter = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "hyp_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    void addSupportingEvidence(const std::string& evidence_id) {
        if (std::find(supporting_evidence_ids.begin(), supporting_evidence_ids.end(), evidence_id) == supporting_evidence_ids.end()) {
            supporting_evidence_ids.push_back(evidence_id);
            updated_at = Timestamp::now();
        }
    }
    
    void addRefutingEvidence(const std::string& evidence_id) {
        if (std::find(refuting_evidence_ids.begin(), refuting_evidence_ids.end(), evidence_id) == refuting_evidence_ids.end()) {
            refuting_evidence_ids.push_back(evidence_id);
            updated_at = Timestamp::now();
        }
    }
    
    void confirm() {
        status = InvestigationStatus::Confirmed;
        confirmed_at = Timestamp::now();
        updated_at = confirmed_at;
        confidence_score = 1.0;
    }
    
    void reject() {
        status = InvestigationStatus::Rejected;
        rejected_at = Timestamp::now();
        updated_at = rejected_at;
        confidence_score = 0.0;
    }
    
    static InvestigationStatus stringToStatus(const std::string& str) {
        static const std::map<std::string, InvestigationStatus> map = {
            {"Open", InvestigationStatus::Open},
            {"InProgress", InvestigationStatus::InProgress},
            {"Confirmed", InvestigationStatus::Confirmed},
            {"Rejected", InvestigationStatus::Rejected},
            {"NeedsEvidence", InvestigationStatus::NeedsEvidence},
            {"Blocked", InvestigationStatus::Blocked},
            {"Superseded", InvestigationStatus::Superseded}
        };
        auto it = map.find(str);
        return it != map.end() ? it->second : InvestigationStatus::Open;
    }
    
    static std::string statusToString(InvestigationStatus status) {
        switch (status) {
            case InvestigationStatus::Open: return "Open";
            case InvestigationStatus::InProgress: return "InProgress";
            case InvestigationStatus::Confirmed: return "Confirmed";
            case InvestigationStatus::Rejected: return "Rejected";
            case InvestigationStatus::NeedsEvidence: return "NeedsEvidence";
            case InvestigationStatus::Blocked: return "Blocked";
            case InvestigationStatus::Superseded: return "Superseded";
        }
        return "Open";
    }
};

/**
 * Hypothesis tracker - manages investigation state
 */
class HypothesisTracker {
public:
    Hypothesis& create(const std::string& title, const std::string& description = "") {
        Hypothesis hyp;
        hyp.title = title;
        hyp.description = description;
        m_hypotheses[hyp.id] = std::move(hyp);
        return m_hypotheses[hyp.id];
    }
    
    bool remove(const std::string& id) {
        return m_hypotheses.erase(id) > 0;
    }
    
    std::optional<Hypothesis> find(const std::string& id) const {
        auto it = m_hypotheses.find(id);
        if (it != m_hypotheses.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<Hypothesis> findByStatus(InvestigationStatus status) const {
        std::vector<Hypothesis> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            if (hyp.status == status) {
                result.push_back(hyp);
            }
        }
        return result;
    }
    
    std::vector<Hypothesis> getActive() const {
        std::vector<Hypothesis> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            if (hyp.status == InvestigationStatus::Open || 
                hyp.status == InvestigationStatus::InProgress ||
                hyp.status == InvestigationStatus::NeedsEvidence) {
                result.push_back(hyp);
            }
        }
        return result;
    }
    
    std::vector<Hypothesis> getConfirmed() const {
        return findByStatus(InvestigationStatus::Confirmed);
    }
    
    std::vector<Hypothesis> getRejected() const {
        return findByStatus(InvestigationStatus::Rejected);
    }
    
    /**
     * Check if a similar hypothesis already exists to prevent duplicate investigations
     */
    std::optional<Hypothesis> findSimilar(const std::string& title, double threshold = 0.7) const {
        for (const auto& [id, hyp] : m_hypotheses) {
            double similarity = calculateSimilarity(title, hyp.title);
            if (similarity >= threshold) {
                return hyp;
            }
        }
        return std::nullopt;
    }
    
    std::vector<Hypothesis> all() const {
        std::vector<Hypothesis> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            result.push_back(hyp);
        }
        return result;
    }
    
    size_t size() const { return m_hypotheses.size(); }
    bool empty() const { return m_hypotheses.empty(); }
    
private:
    std::map<std::string, Hypothesis> m_hypotheses;
    
    /**
     * Simple text similarity using bigram overlap
     */
    double calculateSimilarity(const std::string& a, const std::string& b) const {
        auto bigrams = [](const std::string& s) -> std::set<std::string> {
            std::set<std::string> result;
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            for (size_t i = 0; i + 1 < lower.length(); ++i) {
                result.insert(lower.substr(i, 2));
            }
            return result;
        };
        
        auto bgA = bigrams(a);
        auto bgB = bigrams(b);
        
        if (bgA.empty() && bgB.empty()) return 1.0;
        if (bgA.empty() || bgB.empty()) return 0.0;
        
        std::set<std::string> intersection;
        std::set_intersection(bgA.begin(), bgA.end(), bgB.begin(), bgB.end(),
                             std::inserter(intersection, intersection.begin()));
        
        std::set<std::string> union_set;
        std::set_union(bgA.begin(), bgA.end(), bgB.begin(), bgB.end(),
                      std::inserter(union_set, union_set.begin()));
        
        return static_cast<double>(intersection.size()) / union_set.size();
    }
};

// ============================================================================
// Experiment Recording System
// ============================================================================

/**
 * Build configuration capture
 */
struct BuildConfiguration {
    std::string compiler;
    std::string compiler_version;
    std::string build_type;  // Debug, Release, RelWithDebInfo
    std::string cmake_args;
    std::string cxx_standard;
    std::vector<std::string> defines;
    std::vector<std::string> include_paths;
    std::vector<std::string> linked_libraries;
    std::string git_branch;
    std::string git_commit_hash;
    std::string git_commit_message;
    bool is_dirty;  // Uncommitted changes
    
    std::map<std::string, std::string> toMap() const {
        return {
            {"compiler", compiler},
            {"compiler_version", compiler_version},
            {"build_type", build_type},
            {"cmake_args", cmake_args},
            {"cxx_standard", cxx_standard},
            {"git_branch", git_branch},
            {"git_commit_hash", git_commit_hash},
            {"git_commit_message", git_commit_message},
            {"is_dirty", is_dirty ? "true" : "false"}
        };
    }
};

/**
 * Environment snapshot
 */
struct EnvironmentSnapshot {
    std::map<std::string, std::string> variables;
    std::string os_name;
    std::string os_version;
    std::string architecture;
    std::string hostname;
    std::string username;
    int cpu_count;
    size_t total_memory_mb;
    std::string timestamp;
    
    EnvironmentSnapshot() : cpu_count(0), total_memory_mb(0) {
        timestamp = Timestamp::now();
    }
    
    std::optional<std::string> getVar(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

/**
 * Crash state capture
 */
struct CrashState {
    std::string signal_type;
    std::string signal_number;
    std::string fault_address;
    std::vector<std::string> stack_trace;
    std::vector<std::string> registers;
    std::string module_name;
    std::string offset;
    std::string timestamp;
    std::string crash_log_path;
    
    CrashState() {
        timestamp = Timestamp::now();
    }
    
    bool hasStackTrace() const { return !stack_trace.empty(); }
    bool hasRegisters() const { return !registers.empty(); }
};

/**
 * Complete experiment record
 */
struct ExperimentRecord {
    std::string id;
    std::string title;
    std::string description;
    std::string created_at;
    std::string updated_at;
    std::string completed_at;
    
    BuildConfiguration build_config;
    EnvironmentSnapshot environment;
    CrashState crash_state;
    
    EvidenceCollection evidences;
    HypothesisTracker hypotheses;
    
    std::string issue_reference;  // GitHub issue, JIRA, etc.
    std::vector<std::string> tags;
    std::string status;  // running, completed, failed, aborted
    
    std::string exp_package_path;
    
    ExperimentRecord() {
        id = generateId();
        created_at = Timestamp::now();
        updated_at = created_at;
        status = "running";
    }
    
    static std::string generateId() {
        static uint64_t counter = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "exp_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    void markCompleted() {
        status = "completed";
        completed_at = Timestamp::now();
        updated_at = completed_at;
    }
    
    void markFailed(const std::string& reason = "") {
        status = "failed";
        if (!reason.empty()) {
            description += "\nFailure: " + reason;
        }
        updated_at = Timestamp::now();
    }
    
    void markAborted() {
        status = "aborted";
        updated_at = Timestamp::now();
    }
};

// ============================================================================
// History Search System
// ============================================================================

/**
 * Entry in experiment history
 */
struct HistoryEntry {
    std::string experiment_id;
    std::string title;
    std::string status;
    std::string timestamp;
    std::vector<std::string> tags;
    std::string summary;
    fs::path package_path;
    
    /**
     * Check if this entry matches a search query
     */
    bool matchesQuery(const std::string& query) const {
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        std::string combined = title + " " + summary + " ";
        for (const auto& tag : tags) {
            combined += tag + " ";
        }
        std::transform(combined.begin(), combined.end(), combined.begin(), ::tolower);
        
        return combined.find(lower_query) != std::string::npos;
    }
    
    /**
     * Calculate relevance score for a query (0.0 to 1.0)
     */
    double relevanceScore(const std::string& query) const {
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        // Exact title match gets highest score
        std::string lower_title = title;
        std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
        if (lower_title == lower_query) return 1.0;
        
        // Title contains query
        if (lower_title.find(lower_query) != std::string::npos) return 0.9;
        
        // Check tags
        for (const auto& tag : tags) {
            std::string lower_tag = tag;
            std::transform(lower_tag.begin(), lower_tag.end(), lower_tag.begin(), ::tolower);
            if (lower_tag == lower_query) return 0.8;
            if (lower_tag.find(lower_query) != std::string::npos) return 0.7;
        }
        
        // Summary match
        std::string lower_summary = summary;
        std::transform(lower_summary.begin(), lower_summary.end(), lower_summary.begin(), ::tolower);
        if (lower_summary.find(lower_query) != std::string::npos) return 0.5;
        
        return 0.0;
    }
};

/**
 * Duplicate detection warning
 */
struct DuplicateWarning {
    std::string existing_experiment_id;
    std::string existing_title;
    std::string existing_status;
    double similarity_score;
    std::string reason;
    std::string recommendation;
};

/**
 * Upstream fix notification
 */
struct UpstreamFixNotification {
    std::string fix_reference;  // PR URL, commit hash, etc.
    std::string title;
    std::string description;
    std::string fixed_in_version;
    std::string detected_at;
    
    UpstreamFixNotification() {
        detected_at = Timestamp::now();
    }
};

// ============================================================================
// Report Generation System
// ============================================================================

/**
 * Timeline event for reports
 */
struct TimelineEvent {
    std::string timestamp;
    std::string event_type;
    std::string description;
    std::string reference_id;  // Evidence ID, hypothesis ID, etc.
    std::map<std::string, std::string> metadata;
    
    static TimelineEvent evidenceAdded(const Evidence& e) {
        TimelineEvent evt;
        evt.timestamp = e.timestamp;
        evt.event_type = "evidence_added";
        evt.description = "Evidence captured: " + e.description;
        evt.reference_id = e.id;
        evt.metadata = e.toMap();
        return evt;
    }
    
    static TimelineEvent hypothesisCreated(const Hypothesis& h) {
        TimelineEvent evt;
        evt.timestamp = h.created_at;
        evt.event_type = "hypothesis_created";
        evt.description = "Hypothesis proposed: " + h.title;
        evt.reference_id = h.id;
        return evt;
    }
    
    static TimelineEvent hypothesisConfirmed(const Hypothesis& h) {
        TimelineEvent evt;
        evt.timestamp = h.confirmed_at;
        evt.event_type = "hypothesis_confirmed";
        evt.description = "Root cause confirmed: " + h.title;
        evt.reference_id = h.id;
        return evt;
    }
    
    static TimelineEvent hypothesisRejected(const Hypothesis& h) {
        TimelineEvent evt;
        evt.timestamp = h.rejected_at;
        evt.event_type = "hypothesis_rejected";
        evt.description = "Hypothesis rejected: " + h.title;
        evt.reference_id = h.id;
        return evt;
    }
};

/**
 * Final root cause report
 */
struct RootCauseReport {
    std::string experiment_id;
    std::string generated_at;
    std::string title;
    std::string executive_summary;
    
    std::string root_cause_description;
    std::string confirmed_hypothesis_id;
    double confidence_level;
    
    std::vector<TimelineEvent> timeline;
    std::vector<Evidence> key_evidence;
    std::vector<Hypothesis> all_hypotheses;
    std::vector<Hypothesis> rejected_hypotheses;
    
    std::string recommended_fix;
    std::string verification_steps;
    std::string lessons_learned;
    
    std::vector<UpstreamFixNotification> upstream_fixes;
    std::vector<DuplicateWarning> duplicates_found;
    
    std::map<std::string, std::string> metadata;
    
    RootCauseReport() : confidence_level(0.0) {
        generated_at = Timestamp::now();
    }
    
    /**
     * Validate that report has minimum required fields populated
     */
    bool isValid() const {
        return !experiment_id.empty() &&
               !root_cause_description.empty() &&
               !confirmed_hypothesis_id.empty() &&
               confidence_level > 0.0 &&
               !key_evidence.empty();
    }
};

} // namespace debug_intelligence
