/**
 * Root Cause Report Generator Module
 * 
 * Generates comprehensive EXP reports including:
 * - Timeline of investigation
 * - Evidence summary
 * - Hypothesis tracking (confirmed/rejected)
 * - Final root cause analysis
 * - Recommendations
 */

#pragma once

#include "debug_intelligence.hpp"
#include <iomanip>
#include <numeric>

namespace debug_intelligence {

/**
 * Report configuration options
 */
struct ReportConfig {
    bool include_full_evidence;      // Include complete evidence content
    bool include_raw_logs;           // Include raw log files
    bool include_timeline;           // Include investigation timeline
    bool include_rejected;           // Include rejected hypotheses
    bool include_recommendations;    // Include fix recommendations
    bool ai_friendly_format;         // Format optimized for LLM parsing
    int max_evidence_detail_length;  // Truncate long evidence content
    
    static ReportConfig detailed() {
        return {true, true, true, true, true, true, 10000};
    }
    
    static ReportConfig summary() {
        return {false, false, true, true, true, true, 500};
    }
    
    static ReportConfig minimal() {
        return {false, false, false, false, true, false, 200};
    }
    
    static ReportConfig defaultConfig() {
        return {true, false, true, true, true, true, 2000};
    }
};

/**
 * Report generation statistics
 */
struct ReportStats {
    int total_hypotheses;
    int confirmed_hypotheses;
    int rejected_hypotheses;
    int open_hypotheses;
    size_t total_evidence;
    size_t key_evidence_count;
    size_t timeline_events;
    size_t report_size_bytes;
    
    double investigation_duration_hours;  // Time from first to last event
    
    std::string toString() const {
        std::stringstream ss;
        ss << "Report Statistics:\n";
        ss << "  Hypotheses: " << total_hypotheses << " total (" 
           << confirmed_hypotheses << " confirmed, "
           << rejected_hypotheses << " rejected, "
           << open_hypotheses << " open)\n";
        ss << "  Evidence: " << total_evidence << " total (" << key_evidence_count << " key)\n";
        ss << "  Timeline Events: " << timeline_events << "\n";
        ss << "  Investigation Duration: " << std::fixed << std::setprecision(2) 
           << investigation_duration_hours << " hours\n";
        ss << "  Report Size: " << report_size_bytes << " bytes\n";
        return ss.str();
    }
};

/**
 * Root Cause Report Generator
 */
class ReportGenerator {
public:
    explicit ReportGenerator(const ReportConfig& config = ReportConfig::defaultConfig())
        : m_config(config) {}
    
    /**
     * Generate complete root cause report from experiment record
     */
    RootCauseReport generateReport(const ExperimentRecord& record) const {
        RootCauseReport report;
        
        report.experiment_id = record.id;
        report.title = record.title.empty() ? "Debug Investigation" : record.title;
        report.generated_at = Timestamp::now();
        
        // Build timeline from evidence and hypotheses
        buildTimeline(record, report);
        
        // Collect all hypotheses
        collectHypotheses(record, report);
        
        // Identify key evidence (high severity or attached to confirmed hypothesis)
        identifyKeyEvidence(record, report);
        
        // Determine root cause if we have a confirmed hypothesis
        determineRootCause(report);
        
        // Generate recommendations
        generateRecommendations(record, report);
        
        // Calculate metadata
        calculateStats(record, report);
        
        return report;
    }
    
    /**
     * Generate report as formatted text (for CLI output)
     */
    std::string generateTextReport(const RootCauseReport& report) const {
        std::stringstream output;
        
        // Header
        output << "=" << std::string(78, '=') << "\n";
        output << "  ROOT CAUSE ANALYSIS REPORT\n";
        output << "  Experiment: " << report.experiment_id << "\n";
        output << "  Generated: " << report.generated_at << "\n";
        output << "=" << std::string(78, '=') << "\n\n";
        
        // Executive Summary
        output << "EXECUTIVE SUMMARY\n";
        output << "-" << std::string(40, '-') << "\n";
        output << report.executive_summary << "\n\n";
        
        // Root Cause
        if (!report.root_cause_description.empty()) {
            output << "ROOT CAUSE\n";
            output << "-" << std::string(40, '-') << "\n";
            output << report.root_cause_description << "\n";
            output << "Confidence Level: " << std::fixed << std::setprecision(1) 
                   << (report.confidence_level * 100.0) << "%\n";
            output << "Confirmed Hypothesis ID: " << report.confirmed_hypothesis_id << "\n\n";
        }
        
        // Timeline
        if (m_config.include_timeline && !report.timeline.empty()) {
            output << "INVESTIGATION TIMELINE\n";
            output << "-" << std::string(40, '-') << "\n";
            
            for (const auto& event : report.timeline) {
                output << "[" << event.timestamp << "] ";
                output << "[" << event.event_type << "] ";
                output << event.description << "\n";
                
                if (m_config.ai_friendly_format && !event.metadata.empty()) {
                    for (const auto& [k, v] : event.metadata) {
                        output << "  " << k << ": " << truncateString(v, m_config.max_evidence_detail_length) << "\n";
                    }
                }
            }
            output << "\n";
        }
        
        // Key Evidence
        if (!report.key_evidence.empty()) {
            output << "KEY EVIDENCE\n";
            output << "-" << std::string(40, '-') << "\n";
            
            for (size_t i = 0; i < report.key_evidence.size(); ++i) {
                const auto& evd = report.key_evidence[i];
                output << i + 1 << ". [" << Evidence::severityToString(evd.severity) << "] " 
                       << evd.description << "\n";
                
                if (m_config.include_full_evidence) {
                    output << "   Content: " << truncateString(evd.content, m_config.max_evidence_detail_length) << "\n";
                }
                output << "   Source: " << evd.source_path << "\n";
                output << "   Verified: " << (evd.verified ? "Yes" : "No") << "\n";
                output << "\n";
            }
        }
        
        // Confirmed Hypothesis Details
        if (!report.all_hypotheses.empty()) {
            output << "HYPOTHESIS ANALYSIS\n";
            output << "-" << std::string(40, '-') << "\n";
            
            for (const auto& hyp : report.all_hypotheses) {
                if (hyp.status == InvestigationStatus::Confirmed) {
                    output << "[CONFIRMED] " << hyp.title << "\n";
                    output << "  Description: " << hyp.description << "\n";
                    output << "  Confidence: " << std::fixed << std::setprecision(1) 
                           << (hyp.confidence_score * 100.0) << "%\n";
                    
                    if (!hyp.supporting_evidence_ids.empty()) {
                        output << "  Supporting Evidence: " << hyp.supporting_evidence_ids.size() << " items\n";
                    }
                    output << "\n";
                }
            }
        }
        
        // Rejected Hypotheses
        if (m_config.include_rejected && !report.rejected_hypotheses.empty()) {
            output << "REJECTED HYPOTHESES\n";
            output << "-" << std::string(40, '-') << "\n";
            
            for (const auto& hyp : report.rejected_hypotheses) {
                output << "[REJECTED] " << hyp.title << "\n";
                output << "  Reason: " << (hyp.notes.empty() ? "Not specified" : hyp.notes) << "\n";
                
                if (!hyp.refuting_evidence_ids.empty()) {
                    output << "  Refuting Evidence: " << hyp.refuting_evidence_ids.size() << " items\n";
                }
                output << "\n";
            }
        }
        
        // Recommendations
        if (m_config.include_recommendations && !report.recommended_fix.empty()) {
            output << "RECOMMENDATIONS\n";
            output << "-" << std::string(40, '-') << "\n";
            output << report.recommended_fix << "\n\n";
            
            if (!report.verification_steps.empty()) {
                output << "VERIFICATION STEPS:\n";
                output << report.verification_steps << "\n\n";
            }
        }
        
        // Lessons Learned
        if (!report.lessons_learned.empty()) {
            output << "LESSONS LEARNED\n";
            output << "-" << std::string(40, '-') << "\n";
            output << report.lessons_learned << "\n\n";
        }
        
        // Statistics
        if (report.metadata.count("stats")) {
            output << "STATISTICS\n";
            output << "-" << std::string(40, '-') << "\n";
            output << report.metadata.at("stats") << "\n";
        }
        
        // Footer
        output << "=" << std::string(78, '=') << "\n";
        output << "End of Report | Debug Intelligence Layer v" << VERSION << "\n";
        
        return output.str();
    }
    
    /**
     * Generate report as JSON (for AI consumption)
     */
    std::string generateJsonReport(const RootCauseReport& report) const {
        std::stringstream json;
        
        json << "{\n";
        
        // Basic info
        json << "  \"report_version\": \"1.0\",\n";
        json << "  \"experiment_id\": \"" << escapeJson(report.experiment_id) << "\",\n";
        json << "  \"generated_at\": \"" << escapeJson(report.generated_at) << "\",\n";
        json << "  \"title\": \"" << escapeJson(report.title) << "\",\n";
        
        // Summary section
        json << "  \"summary\": {\n";
        json << "    \"executive_summary\": \"" << escapeJson(report.executive_summary) << "\",\n";
        json << "    \"root_cause\": \"" << escapeJson(report.root_cause_description) << "\",\n";
        json << "    \"confidence_level\": " << report.confidence_level << ",\n";
        json << "    \"confirmed_hypothesis_id\": \"" << escapeJson(report.confirmed_hypothesis_id) << "\"\n";
        json << "  },\n";
        
        // Timeline
        if (m_config.include_timeline) {
            json << "  \"timeline\": [\n";
            for (size_t i = 0; i < report.timeline.size(); ++i) {
                const auto& evt = report.timeline[i];
                json << "    {\n";
                json << "      \"timestamp\": \"" << escapeJson(evt.timestamp) << "\",\n";
                json << "      \"type\": \"" << escapeJson(evt.event_type) << "\",\n";
                json << "      \"description\": \"" << escapeJson(evt.description) << "\"\n";
                json << "    }";
                if (i < report.timeline.size() - 1) json << ",";
                json << "\n";
            }
            json << "  ],\n";
        }
        
        // Key evidence
        json << "  \"key_evidence\": [\n";
        for (size_t i = 0; i < report.key_evidence.size(); ++i) {
            const auto& evd = report.key_evidence[i];
            json << "    {\n";
            json << "      \"id\": \"" << escapeJson(evd.id) << "\",\n";
            json << "      \"type\": \"" << Evidence::evidenceTypeToString(evd.type) << "\",\n";
            json << "      \"description\": \"" << escapeJson(evd.description) << "\",\n";
            json << "      \"severity\": \"" << Evidence::severityToString(evd.severity) << "\",\n";
            if (m_config.include_full_evidence) {
                json << "      \"content\": \"" << escapeJson(truncateString(evd.content, m_config.max_evidence_detail_length)) << "\",\n";
            }
            json << "      \"verified\": " << (evd.verified ? "true" : "false") << "\n";
            json << "    }";
            if (i < report.key_evidence.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Hypotheses summary
        json << "  \"hypotheses\": {\n";
        json << "    \"confirmed\": " << countByStatus(report.all_hypotheses, InvestigationStatus::Confirmed) << ",\n";
        json << "    \"rejected\": " << countByStatus(report.all_hypotheses, InvestigationStatus::Rejected) << ",\n";
        json << "    \"open\": " << countByStatus(report.all_hypotheses, InvestigationStatus::Open) + 
                              countByStatus(report.all_hypotheses, InvestigationStatus::InProgress) << ",\n";
        json << "    \"total\": " << report.all_hypotheses.size() << "\n";
        json << "  },\n";
        
        // Rejected hypotheses detail
        if (m_config.include_rejected && !report.rejected_hypotheses.empty()) {
            json << "  \"rejected_hypotheses\": [\n";
            for (size_t i = 0; i < report.rejected_hypotheses.size(); ++i) {
                const auto& hyp = report.rejected_hypotheses[i];
                json << "    {\n";
                json << "      \"id\": \"" << escapeJson(hyp.id) << "\",\n";
                json << "      \"title\": \"" << escapeJson(hyp.title) << "\",\n";
                json << "      \"reason\": \"" << escapeJson(hyp.notes) << "\"\n";
                json << "    }";
                if (i < report.rejected_hypotheses.size() - 1) json << ",";
                json << "\n";
            }
            json << "  ],\n";
        }
        
        // Recommendations
        if (m_config.include_recommendations) {
            json << "  \"recommendations\": {\n";
            json << "    \"fix\": \"" << escapeJson(report.recommended_fix) << "\",\n";
            json << "    \"verification\": \"" << escapeJson(report.verification_steps) << "\",\n";
            json << "    \"lessons\": \"" << escapeJson(report.lessons_learned) << "\"\n";
            json << "  },\n";
        }
        
        // Upstream fixes
        if (!report.upstream_fixes.empty()) {
            json << "  \"upstream_fixes\": [\n";
            for (size_t i = 0; i < report.upstream_fixes.size(); ++i) {
                const auto& fix = report.upstream_fixes[i];
                json << "    {\n";
                json << "      \"reference\": \"" << escapeJson(fix.fix_reference) << "\",\n";
                json << "      \"title\": \"" << escapeJson(fix.title) << "\"\n";
                json << "    }";
                if (i < report.upstream_fixes.size() - 1) json << ",";
                json << "\n";
            }
            json << "  ],\n";
        }
        
        // Metadata
        json << "  \"metadata\": {\n";
        for (auto it = report.metadata.begin(); it != report.metadata.end(); ++it) {
            json << "    \"" << escapeJson(it->first) << "\": \"" << escapeJson(it->second) << "\"";
            if (std::next(it) != report.metadata.end()) json << ",";
            json << "\n";
        }
        json << "  }\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * Save report to file
     */
    bool saveReport(const RootCauseReport& report, const fs::path& output_path, bool as_json = false) const {
        try {
            std::ofstream file(output_path);
            if (!file.is_open()) return false;
            
            if (as_json) {
                file << generateJsonReport(report);
            } else {
                file << generateTextReport(report);
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * Get report statistics
     */
    ReportStats getStats(const RootCauseReport& report) const {
        ReportStats stats;
        
        stats.total_hypotheses = report.all_hypotheses.size();
        stats.confirmed_hypotheses = countByStatus(report.all_hypotheses, InvestigationStatus::Confirmed);
        stats.rejected_hypotheses = countByStatus(report.all_hypotheses, InvestigationStatus::Rejected);
        stats.open_hypotheses = countByStatus(report.all_hypotheses, InvestigationStatus::Open) +
                               countByStatus(report.all_hypotheses, InvestigationStatus::InProgress);
        stats.total_evidence = report.key_evidence.size();  // Would need full experiment for total
        stats.key_evidence_count = report.key_evidence.size();
        stats.timeline_events = report.timeline.size();
        
        // Estimate report size
        std::string report_str = generateTextReport(report);
        stats.report_size_bytes = report_str.length();
        
        // Calculate duration from timeline
        if (report.timeline.size() >= 2) {
            // Simple calculation - would need proper time parsing for accuracy
            stats.investigation_duration_hours = 0.0;  // Placeholder
        }
        
        return stats;
    }

private:
    ReportConfig m_config;
    
    void buildTimeline(const ExperimentRecord& record, RootCauseReport& report) const {
        // Add creation event
        TimelineEvent create_evt;
        create_evt.timestamp = record.created_at;
        create_evt.event_type = "experiment_started";
        create_evt.description = "Experiment started: " + record.title;
        create_evt.reference_id = record.id;
        report.timeline.push_back(create_evt);
        
        // Add evidence events
        for (const auto& evd : record.evidences.all()) {
            report.timeline.push_back(TimelineEvent::evidenceAdded(evd));
        }
        
        // Add hypothesis events
        for (const auto& hyp : record.hypotheses.all()) {
            report.timeline.push_back(TimelineEvent::hypothesisCreated(hyp));
            
            if (hyp.status == InvestigationStatus::Confirmed) {
                report.timeline.push_back(TimelineEvent::hypothesisConfirmed(hyp));
            } else if (hyp.status == InvestigationStatus::Rejected) {
                report.timeline.push_back(TimelineEvent::hypothesisRejected(hyp));
            }
        }
        
        // Sort by timestamp
        std::sort(report.timeline.begin(), report.timeline.end(),
                 [](const TimelineEvent& a, const TimelineEvent& b) {
                     return a.timestamp < b.timestamp;
                 });
    }
    
    void collectHypotheses(const ExperimentRecord& record, RootCauseReport& report) const {
        report.all_hypotheses = record.hypotheses.all();
        
        // Separate rejected
        for (const auto& hyp : report.all_hypotheses) {
            if (hyp.status == InvestigationStatus::Rejected || 
                hyp.status == InvestigationStatus::Superseded) {
                report.rejected_hypotheses.push_back(hyp);
            }
        }
    }
    
    void identifyKeyEvidence(const ExperimentRecord& record, RootCauseReport& report) const {
        // Get high-severity evidence
        auto all_evd = record.evidences.all();
        
        // First pass: critical and error severity
        for (const auto& evd : all_evd) {
            if (evd.severity == Severity::Critical || evd.severity == Severity::Error) {
                report.key_evidence.push_back(evd);
            }
        }
        
        // Second pass: evidence attached to confirmed hypotheses
        for (const auto& hyp : record.hypotheses.getConfirmed()) {
            for (const auto& evd_id : hyp.supporting_evidence_ids) {
                auto evd = record.evidences.find(evd_id);
                if (evd) {
                    // Check if already added
                    bool found = false;
                    for (const auto& existing : report.key_evidence) {
                        if (existing.id == evd->id) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        report.key_evidence.push_back(*evd);
                    }
                }
            }
        }
        
        // If no key evidence yet, take some warnings too
        if (report.key_evidence.empty()) {
            for (const auto& evd : all_evd) {
                if (evd.severity >= Severity::Warning) {
                    report.key_evidence.push_back(evd);
                    if (report.key_evidence.size() >= 5) break;  // Limit
                }
            }
        }
    }
    
    void determineRootCause(RootCauseReport& report) const {
        auto confirmed = [&]() -> std::optional<Hypothesis> {
            for (const auto& hyp : report.all_hypotheses) {
                if (hyp.status == InvestigationStatus::Confirmed) {
                    return hyp;
                }
            }
            return std::nullopt;
        }();
        
        if (confirmed) {
            report.root_cause_description = confirmed->description.empty() 
                ? confirmed->title 
                : confirmed->description;
            report.confirmed_hypothesis_id = confirmed->id;
            report.confidence_level = confirmed->confidence_score;
            
            // Build executive summary
            report.executive_summary = "Root cause identified: " + confirmed->title + ". " +
                "Investigation completed with " + std::to_string(report.all_hypotheses.size()) + 
                " hypotheses tested, " + std::to_string(report.rejected_hypotheses.size()) + " rejected.";
        } else if (!report.all_hypotheses.empty()) {
            // No confirmed hypothesis - report status
            report.executive_summary = "Investigation in progress: " + 
                std::to_string(report.all_hypotheses.size()) + " hypotheses under consideration. " +
                "No root cause confirmed yet.";
            report.root_cause_description = "Under investigation";
            report.confidence_level = 0.0;
        } else {
            report.executive_summary = "Experiment recorded but no hypotheses proposed yet.";
            report.root_cause_description = "Not determined";
        }
    }
    
    void generateRecommendations(const ExperimentRecord& record, RootCauseReport& report) const {
        // Find the confirmed hypothesis to base recommendations on
        for (const auto& hyp : record.hypotheses.getConfirmed()) {
            // Extract potential fix from hypothesis description/notes
            if (!hyp.notes.empty()) {
                report.recommended_fix = hyp.notes;
            } else {
                report.recommended_fix = "Address issue: " + hyp.title;
            }
            
            // Generic verification steps
            report.verification_steps = buildVerificationSteps(hyp);
            break;
        }
        
        // Lessons learned template
        if (!report.rejected_hypotheses.empty()) {
            std::stringstream lessons;
            lessons << "Investigation ruled out " << report.rejected_hypotheses.size() 
                    << " incorrect hypotheses before finding root cause.\n";
            
            // List what was learned from rejections
            for (const auto& rej : report.rejected_hypotheses) {
                if (!rej.notes.empty()) {
                    lessons << "- " << rej.notes << "\n";
                }
            }
            
            report.lessons_learned = lessons.str();
        }
    }
    
    std::string buildVerificationSteps(const Hypothesis& hyp) const {
        std::stringstream steps;
        steps << "1. Apply fix addressing: " << hyp.title << "\n";
        steps << "2. Rebuild with configuration matching this experiment\n";
        steps << "3. Reproduce original test case\n";
        steps << "4. Verify issue is resolved\n";
        steps << "5. Run regression tests\n";
        steps << "6. Document findings in EXP package\n";
        return steps.str();
    }
    
    void calculateStats(const ExperimentRecord& record, RootCauseReport& report) const {
        ReportStats stats = getStats(report);
        report.metadata["stats"] = stats.toString();
        report.metadata["generator_version"] = VERSION;
        report.metadata["total_evidence"] = std::to_string(record.evidences.size());
        report.metadata["total_hypotheses"] = std::to_string(record.hypotheses.size());
    }
    
    int countByStatus(const std::vector<Hypothesis>& hypotheses, InvestigationStatus status) const {
        return std::count_if(hypotheses.begin(), hypotheses.end(),
                            [status](const Hypothesis& h) { return h.status == status; });
    }
    
    std::string truncateString(const std::string& str, size_t max_len) const {
        if (str.length() <= max_len) return str;
        return str.substr(0, max_len) + "...[truncated]";
    }
    
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
};

} // namespace debug_intelligence
