/**
 * History Search Assistant Module
 * 
 * Provides intelligent search capabilities for:
 * - Scanning previous experiments/issues/docs
 * - Detecting duplicate investigations
 * - Warning about existing upstream fixes
 * - Suggesting related work
 */

#pragma once

#include "debug_intelligence.hpp"
#include <regex>

namespace debug_intelligence {

/**
 * Search result with relevance scoring
 */
struct SearchResult {
    HistoryEntry entry;
    double relevance_score;
    std::vector<std::string> matched_fields;
    
    bool operator>(const SearchResult& other) const {
        return relevance_score > other.relevance_score;
    }
};

/**
 * Search query configuration
 */
struct SearchQuery {
    std::string text;
    std::vector<std::string> tags;
    std::optional<InvestigationStatus> status_filter;
    std::string date_from;  // ISO format
    std::string date_to;    // ISO format
    int max_results;
    double min_relevance;
    
    static SearchQuery forText(const std::string& text, int max_results = 10) {
        return {text, {}, std::nullopt, "", "", max_results, 0.3};
    }
    
    static SearchQuery forTags(const std::vector<std::string>& tags) {
        return {"", tags, std::nullopt, "", "", 50, 0.0};
    }
};

/**
 * Upstream fix database entry
 */
struct UpstreamFixEntry {
    std::string fix_id;
    std::string pr_url;
    std::string commit_hash;
    std::string title;
    std::string description;
    std::string affected_component;
    std::string fixed_version;
    std::string date_merged;
    std::vector<std::string> related_issues;
    std::vector<std::string> keywords;
    
    /**
     * Check if this fix might be relevant to a search query
     */
    double relevanceToQuery(const std::string& query) const {
        std::string combined = title + " " + description + " " + affected_component + " ";
        for (const auto& kw : keywords) {
            combined += kw + " ";
        }
        
        // Simple keyword matching
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        std::string lower_combined = combined;
        std::transform(lower_combined.begin(), lower_combined.end(), lower_combined.begin(), ::tolower);
        
        if (lower_combined.find(lower_query) != std::string::npos) {
            return 0.9;  // Direct match
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
            return static_cast<double>(matches) / query_words.size();
        }
        
        return 0.0;
    }
    
private:
    static std::vector<std::string> splitWords(const std::string& text) {
        std::vector<std::string> words;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            if (word.length() > 2) {  // Skip short words
                words.push_back(word);
            }
        }
        return words;
    }
};

/**
 * History index for fast lookups
 */
struct HistoryIndex {
    std::map<std::string, std::vector<size_t>> title_keywords;     // keyword -> experiment indices
    std::map<std::string, std::vector<size_t>> tag_index;          // tag -> experiment indices
    std::map<InvestigationStatus, std::vector<size_t>> status_index; // status -> indices
    std::vector<HistoryEntry> entries;
    
    void rebuild(const std::vector<HistoryEntry>& all_entries) {
        entries = all_entries;
        title_keywords.clear();
        tag_index.clear();
        status_index.clear();
        
        for (size_t i = 0; i < entries.size(); ++i) {
            // Index title keywords
            auto keywords = extractKeywords(entries[i].title);
            for (const auto& kw : keywords) {
                title_keywords[kw].push_back(i);
            }
            
            // Index tags
            for (const auto& tag : entries[i].tags) {
                tag_index[tag].push_back(i);
            }
            
            // Index status (parse from status string)
            auto status = parseStatus(entries[i].status);
            status_index[status].push_back(i);
        }
    }
    
private:
    static std::vector<std::string> extractKeywords(const std::string& text) {
        std::vector<std::string> keywords;
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        // Extract meaningful words (3+ chars)
        std::stringstream ss(lower);
        std::string word;
        while (ss >> word) {
            // Clean word of punctuation
            word.erase(std::remove_if(word.begin(), word.end(), 
                     [](char c) { return !std::isalnum(c); }), word.end());
            
            if (word.length() >= 3) {
                keywords.push_back(word);
            }
        }
        
        return keywords;
    }
    
    static InvestigationStatus parseStatus(const std::string& status_str) {
        if (status_str == "completed") return InvestigationStatus::Confirmed;
        if (status_str == "failed" || status_str == "aborted") return InvestigationStatus::Rejected;
        return InvestigationStatus::Open;
    }
};

/**
 * History Search Assistant - Main interface
 */
class HistorySearchAssistant {
public:
    explicit HistorySearchAssistant(const fs::path& experiments_dir)
        : m_experiments_dir(experiments_dir), m_index_built(false) {}
    
    // ========================================================================
    // Index Management
    // ========================================================================
    
    /**
     * Build search index from all EXP packages in directory
     */
    size_t buildIndex() {
        m_history.clear();
        
        if (!fs::exists(m_experiments_dir)) {
            m_last_error = "Experiments directory does not exist";
            return 0;
        }
        
        // Scan for .exp.json files
        for (const auto& entry : fs::directory_iterator(m_experiments_dir)) {
            if (entry.path().extension().string() == EXP_PACKAGE_EXT) {
                auto exp_record = loadExperimentSummary(entry.path());
                if (exp_record) {
                    HistoryEntry hist_entry;
                    hist_entry.experiment_id = exp_record->id;
                    hist_entry.title = exp_record->title;
                    hist_entry.status = exp_record->status;
                    hist_entry.timestamp = exp_record->created_at;
                    hist_entry.tags = exp_record->tags;
                    hist_entry.summary = exp_record->description;
                    hist_entry.package_path = entry.path();
                    
                    m_history.push_back(hist_entry);
                }
            }
        }
        
        // Rebuild index
        m_index.rebuild(m_history);
        m_index_built = true;
        
        return m_history.size();
    }
    
    /**
     * Add single experiment to history
     */
    void addToHistory(const ExperimentRecord& record) {
        HistoryEntry entry;
        entry.experiment_id = record.id;
        entry.title = record.title;
        entry.status = record.status;
        entry.timestamp = record.created_at;
        entry.tags = record.tags;
        entry.summary = record.description;
        // Package path would be set when saved
        
        m_history.push_back(entry);
        m_index_dirty = true;
    }
    
    // ========================================================================
    // Search Operations
    // ========================================================================
    
    /**
     * Search history by text query
     */
    std::vector<SearchResult> search(const SearchQuery& query) const {
        ensureIndexBuilt();
        
        std::vector<SearchResult> results;
        
        for (size_t i = 0; i < m_index.entries.size(); ++i) {
            const auto& entry = m_index.entries[i];
            
            double score = 0.0;
            std::vector<std::string> matched;
            
            // Text matching
            if (!query.text.empty()) {
                score = entry.relevanceScore(query.text);
                if (score > 0) {
                    matched.push_back("text");
                }
            }
            
            // Tag matching
            for (const auto& tag : query.tags) {
                auto it = std::find(entry.tags.begin(), entry.tags.end(), tag);
                if (it != entry.tags.end()) {
                    score = std::max(score, 0.8);
                    matched.push_back("tag:" + tag);
                }
            }
            
            // Status filter
            if (query.status_filter.has_value()) {
                InvestigationStatus entry_status = (entry.status == "completed") ? InvestigationStatus::Confirmed :
                                                   (entry.status == "failed") ? InvestigationStatus::Rejected :
                                                   InvestigationStatus::Open;
                if (entry_status != *query.status_filter) {
                    score = 0.0;
                }
            }
            
            // Apply minimum relevance threshold
            if (score >= query.min_relevance) {
                results.push_back({entry, score, matched});
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
    std::vector<SearchResult> searchText(const std::string& query, int max_results = 10) const {
        return search(SearchQuery::forText(query, max_results));
    }
    
    /**
     * Find experiments by tag
     */
    std::vector<SearchResult> findByTag(const std::string& tag) const {
        return search(SearchQuery::forTags({tag}));
    }
    
    // ========================================================================
    // Duplicate Detection
    // ========================================================================
    
    /**
     * Check if a new experiment would duplicate existing work
     */
    std::vector<DuplicateWarning> checkForDuplicates(
        const std::string& title,
        const std::vector<std::string>& tags,
        double threshold = 0.7) const {
        
        ensureIndexBuilt();
        
        std::vector<DuplicateWarning> warnings;
        
        for (const auto& entry : m_history) {
            double similarity = calculateSimilarity(title, entry.title);
            
            // Also boost similarity for matching tags
            size_t matching_tags = 0;
            for (const auto& tag : tags) {
                if (std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end()) {
                    matching_tags++;
                }
            }
            
            if (!tags.empty()) {
                double tag_match_ratio = static_cast<double>(matching_tags) / tags.size();
                similarity = std::max(similarity, tag_match_ratio * 0.8);
            }
            
            if (similarity >= threshold) {
                DuplicateWarning warn;
                warn.existing_experiment_id = entry.experiment_id;
                warn.existing_title = entry.title;
                warn.existing_status = entry.status;
                warn.similarity_score = similarity;
                
                if (similarity >= 0.9) {
                    warn.reason = "Near-exact match to existing investigation";
                    warn.recommendation = "Review existing experiment before proceeding. Consider adding evidence to it instead.";
                } else if (entry.status == "completed") {
                    warn.reason = "Similar to completed investigation";
                    warn.recommendation = "Check if the completed investigation already solved this issue.";
                } else if (entry.status == "running") {
                    warn.reason = "Similar investigation currently in progress";
                    warn.recommendation = "Coordinate with ongoing investigation or wait for results.";
                } else {
                    warn.reason = "Similar topic previously investigated";
                    warn.recommendation = "Review findings from previous investigation before starting new one.";
                }
                
                warnings.push_back(warn);
            }
        }
        
        return warnings;
    }
    
    /**
     * Quick duplicate check returning only boolean and top match
     */
    std::pair<bool, std::optional<DuplicateWarning>> isDuplicate(
        const std::string& title,
        const std::vector<std::string>& tags = {}) const {
        
        auto warnings = checkForDuplicates(title, tags, 0.8);
        
        if (warnings.empty()) {
            return {false, std::nullopt};
        }
        
        // Return highest similarity warning
        auto max_it = std::max_element(warnings.begin(), warnings.end(),
            [](const DuplicateWarning& a, const DuplicateWarning& b) {
                return a.similarity_score < b.similarity_score;
            });
        
        return {true, *max_it};
    }
    
    // ========================================================================
    // Upstream Fix Detection
    // ========================================================================
    
    /**
     * Load upstream fixes database
     */
    bool loadUpstreamFixes(const fs::path& fixes_file) {
        m_upstream_fixes.clear();
        
        if (!fs::exists(fixes_file)) {
            m_last_error = "Upstream fixes file not found: " + fixes_file.string();
            return false;
        }
        
        // Parse JSON file (simple implementation)
        // In production, use proper JSON parser
        
        // For now, load from a simple line-based format
        std::ifstream file(fixes_file);
        if (!file.is_open()) {
            m_last_error = "Cannot open fixes file: " + fixes_file.string();
            return false;
        }
        
        // This is a placeholder - real implementation would parse JSON
        // For now, we'll add some sample entries that can be customized
        
        return true;
    }
    
    /**
     * Add upstream fix entry manually
     */
    void addUpstreamFix(const UpstreamFixEntry& fix) {
        m_upstream_fixes[fix.fix_id] = fix;
    }
    
    /**
     * Check for upstream fixes related to current investigation
     */
    std::vector<UpstreamFixNotification> findRelevantUpstreamFixes(const std::string& query) const {
        std::vector<UpstreamFixNotification> notifications;
        
        for (const auto& [id, fix] : m_upstream_fixes) {
            double relevance = fix.relevanceToQuery(query);
            
            if (relevance >= 0.5) {
                UpstreamFixNotification notification;
                notification.fix_reference = fix.pr_url.empty() ? fix.commit_hash : fix.pr_url;
                notification.title = fix.title;
                notification.description = fix.description;
                notification.fixed_in_version = fix.fixed_version;
                
                notifications.push_back(notification);
            }
        }
        
        // Sort by relevance (would need to store it)
        // For now, just return in order found
        
        return notifications;
    }
    
    // ========================================================================
    // Statistics & Info
    // ========================================================================
    
    /**
     * Get statistics about indexed history
     */
    std::map<std::string, size_t> getStatistics() const {
        ensureIndexBuilt();
        
        std::map<std::string, size_t> stats;
        stats["total_experiments"] = m_history.size();
        
        size_t completed = 0, running = 0, failed = 0;
        for (const auto& entry : m_history) {
            if (entry.status == "completed") completed++;
            else if (entry.status == "running") running++;
            else failed++;
        }
        
        stats["completed"] = completed;
        stats["in_progress"] = running;
        stats["failed"] = failed;
        stats["upstream_fixes_known"] = m_upstream_fixes.size();
        
        return stats;
    }
    
    /**
     * Get list of all known experiment IDs
     */
    std::vector<std::string> getExperimentIds() const {
        ensureIndexBuilt();
        
        std::vector<std::string> ids;
        ids.reserve(m_history.size());
        
        for (const auto& entry : m_history) {
            ids.push_back(entry.experiment_id);
        }
        
        return ids;
    }
    
    /**
     * Get full history entry by ID
     */
    std::optional<HistoryEntry> getExperimentById(const std::string& id) const {
        ensureIndexBuilt();
        
        for (const auto& entry : m_history) {
            if (entry.experiment_id == id) {
                return entry;
            }
        }
        
        return std::nullopt;
    }
    
    fs::path getExperimentsDir() const { return m_experiments_dir; }
    std::string getLastError() const { return m_last_error; }

private:
    fs::path m_experiments_dir;
    std::vector<HistoryEntry> m_history;
    HistoryIndex m_index;
    std::map<std::string, UpstreamFixEntry> m_upstream_fixes;
    bool m_index_built;
    bool m_index_dirty{false};
    mutable std::string m_last_error;
    
    void ensureIndexBuilt() const {
        if (m_index_dirty && !const_cast<HistorySearchAssistant*>(this)->buildIndex()) {
            // Index build failed, continue with stale data
        }
    }
    
    std::optional<ExperimentRecord> loadExperimentSummary(const fs::path& package_path) const {
        // This would call into ExperimentRecorder::loadExpPackage
        // For now, return minimal summary by parsing just the fields we need
        
        std::ifstream file(package_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        ExperimentRecord record;
        
        // Extract basic fields (same logic as recorder)
        auto extractString = [&content](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            auto pos = content.find(search);
            if (pos == std::string::npos) return "";
            
            pos += search.length();
            auto end = content.find("\"", pos);
            if (end == std::string::npos) return "";
            
            return content.substr(pos, end - pos);
        };
        
        record.id = extractString("id");
        record.title = extractString("title");
        record.description = extractString("description");
        record.status = extractString("status");
        record.created_at = extractString("created_at");
        
        // Extract tags array (simplified)
        auto tags_pos = content.find("\"tags\": [");
        if (tags_pos != std::string::npos) {
            auto end_pos = content.find("]", tags_pos);
            if (end_pos != std::string::npos) {
                std::string tags_section = content.substr(tags_pos, end_pos - tags_pos);
                
                // Extract individual tag strings
                size_t pos = 0;
                while ((pos = tags_section.find('\"', pos)) != std::string::npos) {
                    auto start = pos + 1;
                    auto end = tags_section.find('\"', start);
                    if (end == std::string::npos) break;
                    
                    std::string tag = tags_section.substr(start, end - start);
                    if (!tag.empty() && tag != "," && tag != "tags" && tag != ":") {
                        record.tags.push_back(tag);
                    }
                    pos = end + 1;
                }
            }
        }
        
        if (record.id.empty()) {
            return std::nullopt;
        }
        
        return record;
    }
    
    double calculateSimilarity(const std::string& a, const std::string& b) const {
        // Use bigram overlap like HypothesisTracker
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

} // namespace debug_intelligence
