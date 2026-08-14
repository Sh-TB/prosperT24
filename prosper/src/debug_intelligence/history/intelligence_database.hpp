/**
 * Evidence/Hypothesis Intelligence Database - Priority 5
 * 
 * PROBLEM SOLVED:
 * Thousands of experiments created knowledge, but it was lost.
 * Need searchable history to:
 * - Prevent duplicate investigations
 * - Reuse previous findings
 * - Track rejected hypotheses
 * - Detect patterns across investigations
 * 
 * EXAMPLE QUERIES:
 *   search "IL2CPP init"           → Find similar past experiments
 *   similar_crashes              → Find crashes with same symptoms
 *   previous_rejected_hypotheses → What was already ruled out?
 *   known_upstream_fixes         → Is this already fixed?
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <set>
#include <regex>
#include <atomic>
#include <shared_mutex>
#include <thread>

namespace prosper_debug {
namespace history {

// ============================================================================
// Experiment Record
// ============================================================================

enum class ExperimentStatus : uint8_t {
    Running,
    Completed,
    Failed,
    Abandoned,
    Superseded
};

inline std::string experimentStatusToString(ExperimentStatus status) {
    switch (status) {
        case ExperimentStatus::Running: return "Running";
        case ExperimentStatus::Completed: return "Completed";
        case ExperimentStatus::Failed: return "Failed";
        case ExperimentStatus::Abandoned: return "Abandoned";
        case ExperimentStatus::Superseded: return "Superseded";
    }
    return "Unknown";
}

struct ExperimentRecord {
    std::string id;
    std::string title;
    std::string description;
    
    // Timing
    DebugTimestamp created_at;
    DebugTimestamp updated_at;
    DebugTimestamp completed_at;
    
    ExperimentStatus status{ExperimentStatus::Running};
    
    // Context
    std::string commit_hash;
    std::string branch_name;
    std::string issue_reference;  // GitHub issue, JIRA, etc.
    
    // Build environment
    std::map<std::string, std::string> build_info;
    
    // Results
    std::string result_summary;
    std::string root_cause;  // If found
    bool root_cause_found{false};
    
    // Classification
    std::vector<std::string> tags;
    std::vector<std::string> affected_components;
    Severity severity{Severity::Info};
    
    // Related items
    std::vector<std::string> hypothesis_ids;
    std::vector<std::string> evidence_ids;
    std::string superseded_by;  // If this was superseded by another experiment
    
    // Package location
    fs::path package_path;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "exp_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    ExperimentRecord() {
        id = generateId();
        created_at = DebugTimestamp::now();
        updated_at = created_at;
    }
    
    void markCompleted(const std::string& result = "") {
        status = ExperimentStatus::Completed;
        completed_at = DebugTimestamp::now();
        updated_at = completed_at;
        if (!result.empty()) result_summary = result;
    }
    
    void markFailed(const std::string& reason = "") {
        status = ExperimentStatus::Failed;
        updated_at = DebugTimestamp::now();
        if (!reason.empty()) result_summary = "Failed: " + reason;
    }
    
    void markAbandoned() {
        status = ExperimentStatus::Abandoned;
        updated_at = DebugTimestamp::now();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"title\": \"" << JsonUtils::escapeJson(title) << "\",\n";
        json << "  \"status\": \"" << experimentStatusToString(status) << "\",\n";
        json << "  \"created\": \"" << created_at.toISO8601() << "\",\n";
        if (!commit_hash.empty()) {
            json << "  \"commit\": \"" << JsonUtils::escapeJson(commit_hash) << "\",\n";
        }
        if (!result_summary.empty()) {
            json << "  \"result\": \"" << JsonUtils::escapeJson(result_summary) << "\",\n";
        }
        if (root_cause_found) {
            json << "  \"root_cause\": \"" << JsonUtils::escapeJson(root_cause) << "\"\n";
        }
        
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Hypothesis Record
// ============================================================================

struct HypothesisRecord {
    std::string id;
    std::string experiment_id;  // Parent experiment
    
    std::string title;
    std::string description;
    
    InvestigationStatus status{InvestigationStatus::Open};
    
    // Timing
    DebugTimestamp created_at;
    DebugTimestamp confirmed_at;
    DebugTimestamp rejected_at;
    
    // Evidence
    std::vector<std::string> supporting_evidence_ids;
    std::vector<std::string> refuting_evidence_ids;
    
    // Confidence
    double confidence_score{0.0};
    std::string confidence_reasoning;
    
    // Rejection details (if rejected)
    std::vector<std::string> rejection_reasons;
    
    // Root cause chain (if confirmed)
    std::string root_cause_description;
    std::vector<std::string> causal_evidence_ids;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "hyp_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    HypothesisRecord() {
        id = generateId();
        created_at = DebugTimestamp::now();
    }
    
    void confirm(const std::string& cause = "") {
        status = InvestigationStatus::Confirmed;
        confirmed_at = DebugTimestamp::now();
        confidence_score = 1.0;
        if (!cause.empty()) root_cause_description = cause;
    }
    
    void reject(const std::string& reason = "") {
        status = InvestigationStatus::Rejected;
        rejected_at = DebugTimestamp::now();
        confidence_score = 0.0;
        if (!reason.empty()) rejection_reasons.push_back(reason);
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"title\": \"" << JsonUtils::escapeJson(title) << "\",\n";
        json << "  \"status\": \"" << investigationStatusToString(status) << "\",\n";
        json << "  \"confidence\": " << std::fixed << std::setprecision(2) 
             << confidence_score << ",\n";
        json << "  \"supporting_evidence\": " << supporting_evidence_ids.size() << ",\n";
        json << "  \"refuting_evidence\": " << refuting_evidence_ids.size() << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Upstream Fix Record
// ============================================================================

struct UpstreamFixRecord {
    std::string id;
    
    // Identification
    std::string pr_url;
    std::string pr_title;
    std::string commit_hash;
    std::string author;
    
    // Content
    std::string description;
    std::string fixed_issue_description;
    
    // Status
    bool is_merged{false};
    std::string merged_in_version;
    DebugTimestamp merged_at;
    
    // Applicability
    std::vector<std::string> affected_components;
    std::vector<std::string> keywords;
    std::vector<std::string> related_issues;
    
    // Detection metadata
    DebugTimestamp added_to_database;
    std::string source;  // Manual, automated, etc.
    
    /**
     * Check relevance to a query
     */
    double relevanceToQuery(const std::string& query) const {
        std::string combined = pr_title + " " + description + " " + fixed_issue_description + " ";
        for (const auto& kw : keywords) combined += kw + " ";
        for (const auto& comp : affected_components) combined += comp + " ";
        
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        std::string lower_combined = combined;
        std::transform(lower_combined.begin(), lower_combined.end(), lower_combined.begin(), ::tolower);
        
        // Direct match
        if (lower_combined.find(lower_query) != std::string::npos) {
            return 0.95;
        }
        
        // Word-level matching
        std::vector<std::string> query_words = splitWords(lower_query);
        int matches = 0;
        for (const auto& word : query_words) {
            if (lower_combined.find(word) != std::string::npos) {
                matches++;
            }
        }
        
        if (!query_words.empty()) {
            return static_cast<double>(matches) / query_words.size() * 0.8;
        }
        
        return 0.0;
    }

private:
    static std::vector<std::string> splitWords(const std::string& text) {
        std::vector<std::string> words;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            if (word.length() > 2) words.push_back(word);
        }
        return words;
    }
};

// ============================================================================
// Search Result
// ============================================================================

struct SearchResult {
    double relevance_score;
    std::string matched_fields;
    
    // For experiment results
    std::optional<ExperimentRecord> experiment;
    
    // For hypothesis results
    std::optional<HypothesisRecord> hypothesis;
    
    // For upstream fix results
    std::optional<UpstreamFixRecord> fix;
    
    bool operator>(const SearchResult& other) const {
        return relevance_score > other.relevance_score;
    }
};

// ============================================================================
// Duplicate Detection Warning
// ============================================================================

struct DuplicateWarning {
    std::string existing_id;
    std::string existing_title;
    ExperimentStatus existing_status;
    double similarity_score;
    
    std::string reason;
    std::string recommendation;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"existing_id\": \"" << JsonUtils::escapeJson(existing_id) << "\",\n";
        json << "  \"existing_title\": \"" << JsonUtils::escapeJson(existing_title) << "\",\n";
        json << "  \"status\": \"" << experimentStatusToString(existing_status) << "\",\n";
        json << "  \"similarity\": " << std::fixed << std::setprecision(2) 
             << (similarity_score * 100.0) << "%,\n";
        json << "  \"reason\": \"" << JsonUtils::escapeJson(reason) << "\",\n";
        json << "  \"recommendation\": \"" << JsonUtils::escapeJson(recommendation) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Intelligence Database - Main Class
// ============================================================================

class EvidenceIntelligenceDatabase {
public:
    struct Config {
        size_t max_experiments;
        size_t max_hypotheses_per_experiment;
        size_t max_upstream_fixes;
        bool auto_save;
        fs::path storage_directory;
        
        Config()
            : max_experiments(10000)
            , max_hypotheses_per_experiment(100)
            , max_upstream_fixes(10000)
            , auto_save(true)
            , storage_directory(fs::path("debug_history")) {}
        
        Config(size_t me, size_t mhpe, size_t muf, bool as, fs::path sd)
            : max_experiments(me)
            , max_hypotheses_per_experiment(mhpe)
            , max_upstream_fixes(muf)
            , auto_save(as)
            , storage_directory(sd) {}
        
        static Config defaultConfig() {
            return Config(10000, 100, 10000, true, fs::path("debug_history"));
        }
    };
    
    explicit EvidenceIntelligenceDatabase(const Config& config = Config())
        : m_config(config), m_initialized(false) {}
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    bool initialize() {
        try {
            if (!m_config.storage_directory.empty()) {
                fs::create_directories(m_config.storage_directory);
            }
            
            loadFromStorage();
            m_initialized = true;
            return true;
        } catch (const std::exception& e) {
            m_last_error = e.what();
            return false;
        }
    }
    
    bool isInitialized() const { return m_initialized; }
    std::string getLastError() const { return m_last_error; }
    
    // ========================================================================
    // Experiment Management
    // ========================================================================
    
    /**
     * Create new experiment record
     */
    ExperimentRecord createExperiment(
        const std::string& title,
        const std::string& description = "",
        const std::vector<std::string>& tags = {}) {
        
        ExperimentRecord exp;
        exp.title = title;
        exp.description = description;
        exp.tags = tags;
        
        std::unique_lock lock(m_mutex);
        m_experiments[exp.id] = exp;
        
        if (m_config.auto_save) {
            saveToStorageAsync();
        }
        
        return exp;
    }
    
    /**
     * Update experiment
     */
    bool updateExperiment(const ExperimentRecord& experiment) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_experiments.find(experiment.id);
        if (it == m_experiments.end()) return false;
        
        it->second = experiment;
        it->second.updated_at = DebugTimestamp::now();
        
        if (m_config.auto_save) {
            saveToStorageAsync();
        }
        
        return true;
    }
    
    /**
     * Get experiment by ID
     */
    std::optional<ExperimentRecord> getExperiment(const std::string& id) const {
        std::shared_lock lock(m_mutex);
        
        auto it = m_experiments.find(id);
        if (it != m_experiments.end()) return it->second;
        return std::nullopt;
    }
    
    /**
     * Get all experiments with optional filtering
     */
    std::vector<ExperimentRecord> getExperiments(
        std::optional<ExperimentStatus> status_filter = std::nullopt,
        size_t limit = 100) const {
        
        std::shared_lock lock(m_mutex);
        
        std::vector<ExperimentRecord> result;
        for (const auto& [id, exp] : m_experiments) {
            if (status_filter.has_value() && exp.status != *status_filter) continue;
            result.push_back(exp);
            if (result.size() >= limit) break;
        }
        
        return result;
    }
    
    // ========================================================================
    // Hypothesis Management
    // ========================================================================
    
    /**
     * Create hypothesis within an experiment
     */
    HypothesisRecord createHypothesis(
        const std::string& experiment_id,
        const std::string& title,
        const std::string& description = "") {
        
        HypothesisRecord hyp;
        hyp.experiment_id = experiment_id;
        hyp.title = title;
        hyp.description = description;
        
        std::unique_lock lock(m_mutex);
        m_hypotheses[hyp.id] = hyp;
        
        // Link to experiment
        auto exp_it = m_experiments.find(experiment_id);
        if (exp_it != m_experiments.end()) {
            exp_it->second.hypothesis_ids.push_back(hyp.id);
        }
        
        return hyp;
    }
    
    /**
     * Update hypothesis
     */
    bool updateHypothesis(const HypothesisRecord& hypothesis) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_hypotheses.find(hypothesis.id);
        if (it == m_hypotheses.end()) return false;
        
        it->second = hypothesis;
        return true;
    }
    
    /**
     * Get hypotheses for an experiment
     */
    std::vector<HypothesisRecord> getHypotheses(
        const std::string& experiment_id,
        std::optional<InvestigationStatus> status_filter = std::nullopt) const {
        
        std::shared_lock lock(m_mutex);
        
        std::vector<HypothesisRecord> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            if (hyp.experiment_id != experiment_id) continue;
            if (status_filter.has_value() && hyp.status != *status_filter) continue;
            result.push_back(hyp);
        }
        
        return result;
    }
    
    /**
     * Get all confirmed root causes
     */
    std::vector<HypothesisRecord> getConfirmedRootCauses() const {
        std::shared_lock lock(m_mutex);
        
        std::vector<HypothesisRecord> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            if (hyp.status == InvestigationStatus::Confirmed && 
                !hyp.root_cause_description.empty()) {
                result.push_back(hyp);
            }
        }
        
        return result;
    }
    
    /**
     * Get all rejected hypotheses (to avoid re-investigating)
     */
    std::vector<HypothesisRecord> getRejectedHypotheses(size_t limit = 100) const {
        std::shared_lock lock(m_mutex);
        
        std::vector<HypothesisRecord> result;
        for (const auto& [id, hyp] : m_hypotheses) {
            if (hyp.status == InvestigationStatus::Rejected) {
                result.push_back(hyp);
                if (result.size() >= limit) break;
            }
        }
        
        return result;
    }
    
    // ========================================================================
    // Upstream Fix Management
    // ========================================================================
    
    /**
     * Add known upstream fix
     */
    void addUpstreamFix(const UpstreamFixRecord& fix) {
        std::unique_lock lock(m_mutex);
        m_upstream_fixes[fix.id] = fix;
    }
    
    /**
     * Find relevant upstream fixes for a query
     */
    std::vector<UpstreamFixRecord> findRelevantUpstreamFixes(
        const std::string& query,
        double min_relevance = 0.5,
        size_t limit = 10) const {
        
        std::shared_lock lock(m_mutex);
        
        std::vector<std::pair<double, UpstreamFixRecord>> scored;
        
        for (const auto& [id, fix] : m_upstream_fixes) {
            double relevance = fix.relevanceToQuery(query);
            if (relevance >= min_relevance) {
                scored.push_back({relevance, fix});
            }
        }
        
        // Sort by relevance
        std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Extract top results
        std::vector<UpstreamFixRecord> result;
        for (size_t i = 0; i < scored.size() && i < limit; ++i) {
            result.push_back(scored[i].second);
        }
        
        return result;
    }
    
    // ========================================================================
    // Search Interface
    // ========================================================================
    
    struct SearchQuery {
        std::string text;
        std::vector<std::string> tags;
        std::optional<ExperimentStatus> status_filter;
        std::optional<InvestigationStatus> hypothesis_status_filter;
        int max_results{20};
        double min_relevance{0.3};
        
        static SearchQuery forText(const std::string& text, int max_results = 20) {
            return {text, {}, std::nullopt, std::nullopt, max_results, 0.3};
        }
        
        static SearchQuery forTags(const std::vector<std::string>& tags) {
            return {"", tags, std::nullopt, std::nullopt, 50, 0.0};
        }
    };
    
    /**
     * Search across all data
     */
    std::vector<SearchResult> search(const SearchQuery& query) const {
        std::vector<SearchResult> results;
        
        // Search experiments
        {
            std::shared_lock lock(m_mutex);
            
            for (const auto& [id, exp] : m_experiments) {
                double score = 0.0;
                std::string matched;
                
                if (!query.text.empty()) {
                    score = calculateTextSimilarity(query.text, exp.title + " " + exp.description);
                    if (score > 0) matched = "text";
                }
                
                // Tag matching boosts score
                for (const auto& tag : query.tags) {
                    auto it = std::find(exp.tags.begin(), exp.tags.end(), tag);
                    if (it != exp.tags.end()) {
                        score = std::max(score, 0.8);
                        if (!matched.empty()) matched += "+tag:" + tag;
                        else matched = "tag:" + tag;
                    }
                }
                
                // Status filter
                if (query.status_filter.has_value() && exp.status != *query.status_filter) {
                    score = 0.0;
                }
                
                if (score >= query.min_relevance) {
                    SearchResult sr;
                    sr.relevance_score = score;
                    sr.matched_fields = matched;
                    sr.experiment = exp;
                    results.push_back(sr);
                }
            }
        }
        
        // Sort by relevance
        std::sort(results.begin(), results.end(), std::greater<SearchResult>());
        
        // Limit results
        if (results.size() > static_cast<size_t>(query.max_results)) {
            results.resize(query.max_results);
        }
        
        return results;
    }
    
    /**
     * Simple text search
     */
    std::vector<SearchResult> searchText(const std::string& query, int max_results = 20) const {
        return search(SearchQuery::forText(query, max_results));
    }
    
    // ========================================================================
    // Duplicate Detection
    // ========================================================================
    
    /**
     * Check if a new experiment would duplicate existing work
     */
    std::vector<DuplicateWarning> checkForDuplicates(
        const std::string& title,
        const std::vector<std::string>& tags = {},
        double threshold = 0.7) const {
        
        std::vector<DuplicateWarning> warnings;
        
        std::shared_lock lock(m_mutex);
        
        for (const auto& [id, exp] : m_experiments) {
            double similarity = calculateTextSimilarity(title, exp.title);
            
            // Boost for matching tags
            if (!tags.empty()) {
                size_t matching_tags = 0;
                for (const auto& tag : tags) {
                    if (std::find(exp.tags.begin(), exp.tags.end(), tag) != exp.tags.end()) {
                        matching_tags++;
                    }
                }
                
                if (!exp.tags.empty()) {
                    double tag_ratio = static_cast<double>(matching_tags) / tags.size();
                    similarity = std::max(similarity, tag_ratio * 0.8);
                }
            }
            
            if (similarity >= threshold) {
                DuplicateWarning warn;
                warn.existing_id = id;
                warn.existing_title = exp.title;
                warn.existing_status = exp.status;
                warn.similarity_score = similarity;
                
                if (similarity >= 0.9) {
                    warn.reason = "Near-exact match to existing investigation";
                    warn.recommendation = "Review existing experiment before proceeding.";
                } else if (exp.status == ExperimentStatus::Completed) {
                    warn.reason = "Similar to completed investigation";
                    warn.recommendation = "Check if this experiment already solved your issue.";
                } else if (exp.status == ExperimentStatus::Running) {
                    warn.reason = "Similar investigation currently in progress";
                    warn.recommendation = "Coordinate with ongoing investigation or wait for results.";
                } else {
                    warn.reason = "Similar topic previously investigated";
                    warn.recommendation = "Review findings before starting new work.";
                }
                
                warnings.push_back(warn);
            }
        }
        
        return warnings;
    }
    
    /**
     * Quick duplicate check
     */
    std::pair<bool, std::optional<DuplicateWarning>> isDuplicate(
        const std::string& title,
        const std::vector<std::string>& tags = {}) const {
        
        auto warnings = checkForDuplicates(title, tags, 0.8);
        
        if (warnings.empty()) {
            return {false, std::nullopt};
        }
        
        // Return highest similarity
        auto max_it = std::max_element(warnings.begin(), warnings.end(),
            [](const DuplicateWarning& a, const DuplicateWarning& b) {
                return a.similarity_score < b.similarity_score;
            });
        
        return {true, *max_it};
    }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    struct DatabaseStats {
        size_t total_experiments{0};
        size_t total_hypotheses{0};
        size_t total_upstream_fixes{0};
        
        size_t completed_experiments{0};
        size_t running_experiments{0};
        size_t failed_experiments{0};
        
        size_t confirmed_hypotheses{0};
        size_t rejected_hypotheses{0};
        
        size_t merged_upstream_fixes{0};
        
        std::string toJson() const {
            std::stringstream json;
            json << "{\n";
            json << "  \"experiments\": {\n";
            json << "    \"total\": " << total_experiments << ",\n";
            json << "    \"completed\": " << completed_experiments << ",\n";
            json << "    \"running\": " << running_experiments << ",\n";
            json << "    \"failed\": " << failed_experiments << "\n";
            json << "  },\n";
            json << "  \"hypotheses\": {\n";
            json << "    \"total\": " << total_hypotheses << ",\n";
            json << "    \"confirmed\": " << confirmed_hypotheses << ",\n";
            json << "    \"rejected\": " << rejected_hypotheses << "\n";
            json << "  },\n";
            json << "  \"upstream_fixes\": " << total_upstream_fixes << "\n";
            json << "}\n";
            return json.str();
        }
    };
    
    DatabaseStats getStats() const {
        DatabaseStats stats;
        
        std::shared_lock lock(m_mutex);
        
        stats.total_experiments = m_experiments.size();
        stats.total_hypotheses = m_hypotheses.size();
        stats.total_upstream_fixes = m_upstream_fixes.size();
        
        for (const auto& [id, exp] : m_experiments) {
            switch (exp.status) {
                case ExperimentStatus::Completed: stats.completed_experiments++; break;
                case ExperimentStatus::Running: stats.running_experiments++; break;
                case ExperimentStatus::Failed: stats.failed_experiments++; break;
                default: break;
            }
        }
        
        for (const auto& [id, hyp] : m_hypotheses) {
            switch (hyp.status) {
                case InvestigationStatus::Confirmed: stats.confirmed_hypotheses++; break;
                case InvestigationStatus::Rejected: stats.rejected_hypotheses++; break;
                default: break;
            }
        }
        
        for (const auto& [id, fix] : m_upstream_fixes) {
            if (fix.is_merged) stats.merged_upstream_fixes++;
        }
        
        return stats;
    }

private:
    Config m_config;
    bool m_initialized;
    mutable std::string m_last_error;
    mutable std::shared_mutex m_mutex;
    
    // Storage
    std::map<std::string, ExperimentRecord> m_experiments;
    std::map<std::string, HypothesisRecord> m_hypotheses;
    std::map<std::string, UpstreamFixRecord> m_upstream_fixes;
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    double calculateTextSimilarity(const std::string& a, const std::string& b) const {
        // Bigram overlap similarity
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
    
    void saveToStorageAsync() {
        // In real implementation, would write to disk asynchronously
        // For now, just note that we should save
    }
    
    void loadFromStorage() {
        // In real implementation, would load from JSON files in storage_directory
        // For now, start with empty database
    }
};

} // namespace history
} // namespace prosper_debug
