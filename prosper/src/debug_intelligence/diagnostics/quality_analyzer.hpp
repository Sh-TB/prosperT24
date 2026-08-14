/**
 * Diagnostic Quality Analyzer - Priority 4
 * 
 * PROBLEM SOLVED:
 * A lot of time wasted because diagnostics were added but never executed.
 * 
 * EXAMPLES OF MISLEADING SITUATIONS:
 *   "Added breakpoint, no output, therefore not the issue"
 *   
 *   REALITY:
 *   - Wrong code path (branch not taken)
 *   - Unreachable code (dead path)
 *   - Condition impossible (contradictory preconditions)
 *   - Diagnostic removed during optimization
 *   - Exception thrown before diagnostic reached
 * 
 * REQUIREMENT:
 * Every diagnostic must report:
 *   - Registered: When it was added to the system
 *   - Executed: How many times it ran
 *   - Last execution: When it last ran
 *   - Not reached: Why it might not have run
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <deque>

namespace prosper_debug {
namespace diagnostics {

// ============================================================================
// Diagnostic Status
// ============================================================================

enum class DiagnosticStatus : uint8_t {
    Registered,      // Created and registered in system
    Executed,        // Ran at least once
    Triggered,       // Condition was met (for conditional diagnostics)
    NotReached,      // Code path never entered
    Removed,         // Explicitly removed
    Error            // Diagnostic itself had an error
};

inline std::string diagnosticStatusToString(DiagnosticStatus status) {
    switch (status) {
        case DiagnosticStatus::Registered: return "Registered";
        case DiagnosticStatus::Executed: return "Executed";
        case DiagnosticStatus::Triggered: return "Triggered";
        case DiagnosticStatus::NotReached: return "NotReached";
        case DiagnosticStatus::Removed: return "Removed";
        case DiagnosticStatus::Error: return "Error";
    }
    return "Unknown";
}

// ============================================================================
// Diagnostic Types
// ============================================================================

enum class DiagnosticType : uint8_t {
    // Basic checks
    Assertion,
    ConditionCheck,
    ValueRangeCheck,
    
    // State validation
    StateInvariant,
    PreCondition,
    PostCondition,
    
    // Data integrity
    ChecksumValidation,
    SignatureVerification,
    PointerValidation,
    
    // Timing/ordering
    SequencePoint,
    OrderingConstraint,
    TimeoutCheck,
    
    // Coverage
    CodePathReached,
    FunctionEntered,
    BranchTaken,
    
    // Custom
    CustomDiagnostic,
    Breakpoint,
    Watchpoint,
    LogPoint
};

inline std::string diagnosticTypeToString(DiagnosticType type) {
    switch (type) {
        case DiagnosticType::Assertion: return "Assertion";
        case DiagnosticType::ConditionCheck: return "ConditionCheck";
        case DiagnosticType::ValueRangeCheck: return "ValueRangeCheck";
        case DiagnosticType::StateInvariant: return "StateInvariant";
        case DiagnosticType::PreCondition: return "PreCondition";
        case DiagnosticType::PostCondition: return "PostCondition";
        case DiagnosticType::ChecksumValidation: return "ChecksumValidation";
        case DiagnosticType::SignatureVerification: return "SignatureVerification";
        case DiagnosticType::PointerValidation: return "PointerValidation";
        case DiagnosticType::SequencePoint: return "SequencePoint";
        case DiagnosticType::OrderingConstraint: return "OrderingConstraint";
        case DiagnosticType::TimeoutCheck: return "TimeoutCheck";
        case DiagnosticType::CodePathReached: return "CodePathReached";
        case DiagnosticType::FunctionEntered: return "FunctionEntered";
        case DiagnosticType::BranchTaken: return "BranchTaken";
        case DiagnosticType::CustomDiagnostic: return "CustomDiagnostic";
        case DiagnosticType::Breakpoint: return "Breakpoint";
        case DiagnosticType::Watchpoint: return "Watchpoint";
        case DiagnosticType::LogPoint: return "LogPoint";
    }
    return "Unknown";
}

// ============================================================================
// Why a Diagnostic Might Not Execute
// ============================================================================

enum class NonExecutionReason : uint8_t {
    Unknown,
    
    // Control flow reasons
    ParentBranchNotTaken,
    LoopNeverEntered,
    EarlyReturnBeforeDiagnostic,
    ExceptionThrownBeforeDiagnostic,
    
    // Condition reasons
    ConditionNeverTrue,
    ContradictoryPreconditions,
    ImpossibleState,
    
    // Scope/lifetime reasons
    ObjectDestroyedBeforeDiagnostic,
    ModuleNotLoaded,
    FunctionNeverCalled,
    
    // Optimization reasons
    OptimizedOut,
    DeadCodeEliminated,
    InlinedAway,
    
    // System reasons
    DisabledAtRuntime,
    FilteredOut,
    PriorityTooLow
};

inline std::string nonExecutionReasonToString(NonExecutionReason reason) {
    switch (reason) {
        case NonExecutionReason::Unknown: return "Unknown";
        case NonExecutionReason::ParentBranchNotTaken: return "ParentBranchNotTaken";
        case NonExecutionReason::LoopNeverEntered: return "LoopNeverEntered";
        case NonExecutionReason::EarlyReturnBeforeDiagnostic: return "EarlyReturnBeforeDiagnostic";
        case NonExecutionReason::ExceptionThrownBeforeDiagnostic: return "ExceptionThrownBeforeDiagnostic";
        case NonExecutionReason::ConditionNeverTrue: return "ConditionNeverTrue";
        case NonExecutionReason::ContradictoryPreconditions: return "ContradictoryPreconditions";
        case NonExecutionReason::ImpossibleState: return "ImpossibleState";
        case NonExecutionReason::ObjectDestroyedBeforeDiagnostic: return "ObjectDestroyedBeforeDiagnostic";
        case NonExecutionReason::ModuleNotLoaded: return "ModuleNotLoaded";
        case NonExecutionReason::FunctionNeverCalled: return "FunctionNeverCalled";
        case NonExecutionReason::OptimizedOut: return "OptimizedOut";
        case NonExecutionReason::DeadCodeEliminated: return "DeadCodeEliminated";
        case NonExecutionReason::InlinedAway: return "InlinedAway";
        case NonExecutionReason::DisabledAtRuntime: return "DisabledAtRuntime";
        case NonExecutionReason::FilteredOut: return "FilteredOut";
        case NonExecutionReason::PriorityTooLow: return "PriorityTooLow";
    }
    return "Unknown";
}

// ============================================================================
// Diagnostic Definition
// ============================================================================

struct DiagnosticDefinition {
    std::string id;
    std::string name;
    std::string description;
    DiagnosticType type;
    
    // Location
    SourceLocation location;
    Subsystem subsystem{Subsystem::Unknown};
    
    // Condition (if conditional)
    std::function<bool()> condition;  // When should this run?
    bool is_conditional{false};
    
    // What it checks
    std::function<bool()> check;     // The actual diagnostic logic
    
    // What to do when triggered
    std::function<void(const std::string&)> on_triggered;
    
    // Metadata
    std::map<std::string, std::string> tags;
    std::string category;
    int priority{0};  // Higher = more important
    
    // Expected frequency
    size_t expected_executions_per_session{1};  // 0 = unknown/unlimited
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "diag_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    DiagnosticDefinition() {
        id = generateId();
    }
};

// ============================================================================
// Diagnostic Execution Record
// ============================================================================

struct DiagnosticExecution {
    std::string diagnostic_id;
    DebugTimestamp timestamp;
    uint64_t frame_number{0};
    
    // Result
    DiagnosticStatus status{DiagnosticStatus::Registered};
    bool passed{true};
    std::string result_message;
    std::string failure_detail;
    
    // Context
    std::string thread_id;
    std::map<std::string, std::string> context_data;  // Values at time of execution
    
    // Performance
    uint64_t execution_time_ns{0};
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "exec_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    DiagnosticExecution() {
        timestamp = DebugTimestamp::now();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"diagnostic_id\": \"" << JsonUtils::escapeJson(diagnostic_id) << "\",\n";
        json << "  \"timestamp\": \"" << timestamp.toISO8601() << "\",\n";
        json << "  \"frame\": " << frame_number << ",\n";
        json << "  \"status\": \"" << diagnosticStatusToString(status) << "\",\n";
        json << "  \"passed\": " << (passed ? "true" : "false") << ",\n";
        if (!result_message.empty()) {
            json << "  \"message\": \"" << JsonUtils::escapeJson(result_message) << "\"\n";
        }
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Diagnostic Summary - Current State of a Diagnostic
// ============================================================================

struct DiagnosticSummary {
    DiagnosticDefinition definition;
    
    // Execution history
    std::vector<DiagnosticExecution> executions;
    
    // Aggregated stats
    DiagnosticStatus current_status{DiagnosticStatus::Registered};
    size_t total_executions{0};
    size_t total_passes{0};
    size_t total_failures{0};
    
    // Timing
    DebugTimestamp registration_time;
    DebugTimestamp first_execution;
    DebugTimestamp last_execution;
    
    // If not executing, why?
    NonExecutionReason suspected_reason{NonExecutionReason::Unknown};
    std::string non_execution_analysis;
    
    // Health assessment
    enum class Health {
        Healthy,       // Running as expected
        Suspicious,    // Should have run but hasn't
        Broken,        // Failing consistently
        Unknown         // Not enough data
    } health{Health::Unknown};
    
    std::string health_reason;
    
    double passRate() const {
        return total_executions > 0 ?
            static_cast<double>(total_passes) / total_executions : 1.0;
    }
    
    bool hasEverExecuted() const { return total_executions > 0; }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(definition.id) << "\",\n";
        json << "  \"name\": \"" << JsonUtils::escapeJson(definition.name) << "\",\n";
        json << "  \"type\": \"" << diagnosticTypeToString(definition.type) << "\",\n";
        json << "  \"status\": \"" << diagnosticStatusToString(current_status) << "\",\n";
        json << "  \"executions\": " << total_executions << ",\n";
        json << "  \"passes\": " << total_passes << ",\n";
        json << "  \"failures\": " << total_failures << ",\n";
        json << "  \"pass_rate\": " << std::fixed << std::setprecision(2) 
             << (passRate() * 100.0) << "%,\n";
        
        if (!hasEverExecuted()) {
            json << "  \"not_reached_reason\": \"" 
                 << JsonUtils::escapeJson(nonExecutionReasonToString(suspected_reason)) << "\",\n";
            json << "  \"analysis\": \"" << JsonUtils::escapeJson(non_execution_analysis) << "\",\n";
        }
        
        const char* health_str = "";
        switch (health) {
            case Health::Healthy: health_str = "Healthy"; break;
            case Health::Suspicious: health_str = "Suspicious"; break;
            case Health::Broken: health_str = "Broken"; break;
            case Health::Unknown: health_str = "Unknown"; break;
        }
        json << "  \"health\": \"" << health_str << "\"\n";
        
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Quality Report
// ============================================================================

struct DiagnosticQualityReport {
    // Overall stats
    size_t total_diagnostics{0};
    size_t healthy_count{0};
    size_t suspicious_count{0};
    size_t broken_count{0};
    size_t unknown_count{0};
    size_t never_executed_count{0};
    
    // By subsystem
    std::map<Subsystem, std::pair<size_t, size_t>> by_subsystem;  // total, suspicious
    
    // Problematic diagnostics (need attention)
    std::vector<DiagnosticSummary> suspicious_diagnostics;
    std::vector<DiagnosticSummary> broken_diagnostics;
    std::vector<DiagnosticSummary> never_executed_diagnostics;
    
    // Analysis
    std::vector<std::string> findings;
    std::vector<std::string> recommendations;
    
    double coverage_score{0.0};  // Estimated % of diagnostics actually running
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"total_diagnostics\": " << total_diagnostics << ",\n";
        json << "  \"healthy\": " << healthy_count << ",\n";
        json << "  \"suspicious\": " << suspicious_count << ",\n";
        json << "  \"broken\": " << broken_count << ",\n";
        json << "  \"never_executed\": " << never_executed_count << ",\n";
        json << "  \"coverage_score\": " << std::fixed << std::setprecision(1)
             << (coverage_score * 100.0) << "%,\n";
        
        json << "  \"findings\": [\n";
        for (size_t i = 0; i < findings.size(); ++i) {
            json << "    \"" << JsonUtils::escapeJson(findings[i]) << "\"";
            if (i < findings.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        json << "  \"recommendations\": [\n";
        for (size_t i = 0; i < recommendations.size(); ++i) {
            json << "    \"" << JsonUtils::escapeJson(recommendations[i]) << "\"";
            if (i < recommendations.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        return json.str();
    }
};

// ============================================================================
// Diagnostic Quality Analyzer - Main Class
// ============================================================================

class DiagnosticQualityAnalyzer {
public:
    struct Config {
        bool enabled;
        bool auto_analyze;          // Auto-analyze after each session
        bool track_all_executions;  // Keep full history or just summaries
        size_t max_executions_per_diagnostic;
        size_t max_diagnostics;
        
        // Suspicion thresholds
        size_t min_executions_for_health;  // Need this many before judging
        double suspicious_threshold;      // Below this pass rate = suspicious
        
        Config()
            : enabled(true)
            , auto_analyze(true)
            , track_all_executions(true)
            , max_executions_per_diagnostic(10000)
            , max_diagnostics(10000)
            , min_executions_for_health(3)
            , suspicious_threshold(0.3) {}
        
        Config(bool en, bool aa, bool tae, size_t mepd, size_t md, size_t mefh, double st)
            : enabled(en)
            , auto_analyze(aa)
            , track_all_executions(tae)
            , max_executions_per_diagnostic(mepd)
            , max_diagnostics(md)
            , min_executions_for_health(mefh)
            , suspicious_threshold(st) {}
        
        static Config defaultConfig() {
            return Config(true, true, true, 1000, 5000, 5, 0.5);
        }
    };
    
    explicit DiagnosticQualityAnalyzer(const Config& config = Config())
        : m_config(config), m_enabled(config.enabled) {}
    
    // ========================================================================
    // Control Interface
    // ========================================================================
    
    void enable() { m_enabled.store(true); }
    void disable() { m_enabled.store(false); }
    bool isEnabled() const { return m_enabled.load(); }
    
    void clear() {
        std::unique_lock lock(m_mutex);
        m_diagnostics.clear();
        m_executions.clear();
    }
    
    // ========================================================================
    // Diagnostic Registration
    // ========================================================================
    
    /**
     * Register a new diagnostic
     */
    std::string registerDiagnostic(
        const std::string& name,
        DiagnosticType type,
        std::function<bool()> check_func,
        const SourceLocation& loc = DEBUG_HERE(),
        Subsystem subs = Subsystem::Unknown,
        const std::string& description = "") {
        
        if (!isEnabled()) return "";
        
        DiagnosticDefinition def;
        def.name = name;
        def.type = type;
        def.check = check_func;
        def.location = loc;
        def.subsystem = subs;
        def.description = description;
        
        return registerDiagnostic(def);
    }
    
    /**
     * Register a diagnostic with full definition
     */
    std::string registerDiagnostic(const DiagnosticDefinition& definition) {
        if (!isEnabled()) return "";
        
        std::unique_lock lock(m_mutex);
        
        if (m_diagnostics.size() >= m_config.max_diagnostics) {
            return "";  // At capacity
        }
        
        DiagnosticSummary summary;
        summary.definition = definition;
        summary.registration_time = DebugTimestamp::now();
        summary.current_status = DiagnosticStatus::Registered;
        
        m_diagnostics[definition.id] = summary;
        
        return definition.id;
    }
    
    /**
     * Unregister a diagnostic
     */
    bool unregisterDiagnostic(const std::string& id) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_diagnostics.find(id);
        if (it == m_diagnostics.end()) return false;
        
        it->second.current_status = DiagnosticStatus::Removed;
        // Keep in map for historical analysis, just mark as removed
        return true;
    }
    
    // ========================================================================
    // Execution Interface
    // ========================================================================
    
    /**
     * Execute a diagnostic by ID
     * Returns whether it passed
     */
    std::optional<bool> executeDiagnostic(const std::string& id) {
        if (!isEnabled()) return std::nullopt;
        
        DiagnosticExecution exec;
        exec.diagnostic_id = id;
        exec.thread_id = getCurrentThreadId();
        
        std::unique_lock lock(m_mutex);
        
        auto it = m_diagnostics.find(id);
        if (it == m_diagnostics.end()) {
            exec.status = DiagnosticStatus::Error;
            exec.result_message = "Diagnostic not found";
            
            if (m_config.track_all_executions) {
                m_executions.push_back(exec);
            }
            return std::nullopt;
        }
        
        DiagnosticSummary& summary = it->second;
        lock.unlock();  // Don't hold lock during execution
        
        // Run the diagnostic
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            if (summary.definition.check) {
                exec.passed = summary.definition.check();
            }
            exec.status = DiagnosticStatus::Executed;
            
            if (exec.passed) {
                exec.status = DiagnosticStatus::Triggered;
                if (summary.definition.on_triggered) {
                    summary.definition.on_triggered(summary.definition.id);
                }
            }
        } catch (const std::exception& e) {
            exec.passed = false;
            exec.status = DiagnosticStatus::Error;
            exec.failure_detail = e.what();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        exec.execution_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        
        // Update summary
        lock.lock();
        summary.executions.push_back(exec);
        summary.total_executions++;
        summary.current_status = exec.status;
        summary.last_execution = exec.timestamp;
        
        if (summary.total_executions == 1) {
            summary.first_execution = exec.timestamp;
        }
        
        if (exec.passed) {
            summary.total_passes++;
        } else {
            summary.total_failures++;
        }
        
        // Enforce per-diagnostic limit
        while (summary.executions.size() > m_config.max_executions_per_diagnostic) {
            summary.executions.erase(summary.executions.begin());
        }
        
        if (m_config.track_all_executions) {
            m_executions.push_back(exec);
        }
        
        return exec.passed;
    }
    
    /**
     * Mark a code path as reached (lightweight diagnostic)
     */
    void markCodePathReached(const std::string& id) {
        if (!isEnabled()) return;
        
        DiagnosticExecution exec;
        exec.diagnostic_id = id;
        exec.status = DiagnosticStatus::Executed;
        exec.passed = true;
        exec.result_message = "Code path reached";
        
        std::unique_lock lock(m_mutex);
        
        auto it = m_diagnostics.find(id);
        if (it != m_diagnostics.end()) {
            DiagnosticSummary& summary = it->second;
            summary.executions.push_back(exec);
            summary.total_executions++;
            summary.total_passes++;
            summary.last_execution = exec.timestamp;
            
            if (summary.total_executions == 1) {
                summary.first_execution = exec.timestamp;
            }
        }
    }
    
    /**
     * Record that a diagnostic was not reachable with reason
     */
    void markNotReachable(const std::string& id, NonExecutionReason reason,
                          const std::string& analysis = "") {
        if (!isEnabled()) return;
        
        std::unique_lock lock(m_mutex);
        
        auto it = m_diagnostics.find(id);
        if (it != m_diagnostics.end()) {
            it->second.suspected_reason = reason;
            it->second.non_execution_analysis = analysis;
            it->second.current_status = DiagnosticStatus::NotReached;
        }
    }
    
    // ========================================================================
    // Query Interface
    // ========================================================================
    
    /**
     * Get summary for a specific diagnostic
     */
    std::optional<DiagnosticSummary> getDiagnosticSummary(const std::string& id) const {
        std::shared_lock lock(m_mutex);
        
        auto it = m_diagnostics.find(id);
        if (it != m_diagnostics.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * Get all diagnostics that have never executed
     */
    std::vector<DiagnosticSummary> getNeverExecuted() const {
        std::shared_lock lock(m_mutex);
        
        std::vector<DiagnosticSummary> result;
        for (const auto& [id, summary] : m_diagnostics) {
            if (!summary.hasEverExecuted() && 
                summary.current_status != DiagnosticStatus::Removed) {
                result.push_back(summary);
            }
        }
        return result;
    }
    
    /**
     * Get all suspicious diagnostics (should be running but aren't)
     */
    std::vector<DiagnosticSummary> getSuspicious() const {
        std::shared_lock lock(m_mutex);
        
        std::vector<DiagnosticSummary> result;
        for (const auto& [id, summary] : m_diagnostics) {
            if (summary.health == DiagnosticSummary::Health::Suspicious ||
                summary.health == DiagnosticSummary::Health::Broken) {
                result.push_back(summary);
            }
        }
        return result;
    }
    
    /**
     * Generate quality report
     */
    DiagnosticQualityReport generateReport() const {
        DiagnosticQualityReport report;
        
        std::shared_lock lock(m_mutex);
        
        report.total_diagnostics = m_diagnostics.size();
        
        for (const auto& [id, summary] : m_diagnostics) {
            if (summary.current_status == DiagnosticStatus::Removed) continue;
            
            // Categorize
            switch (summary.health) {
                case DiagnosticSummary::Health::Healthy:
                    report.healthy_count++;
                    break;
                case DiagnosticSummary::Health::Suspicious:
                    report.suspicious_count++;
                    report.suspicious_diagnostics.push_back(summary);
                    break;
                case DiagnosticSummary::Health::Broken:
                    report.broken_count++;
                    report.broken_diagnostics.push_back(summary);
                    break;
                default:
                    report.unknown_count++;
                    break;
            }
            
            if (!summary.hasEverExecuted()) {
                report.never_executed_count++;
                report.never_executed_diagnostics.push_back(summary);
            }
            
            // By subsystem
            auto& sub_stats = report.by_subsystem[summary.definition.subsystem];
            sub_stats.first++;  // Total
            
            if (summary.health == DiagnosticSummary::Health::Suspicious ||
                summary.health == DiagnosticSummary::Health::Broken) {
                sub_stats.second++;  // Problematic
            }
        }
        
        // Calculate coverage score
        size_t executed = report.total_diagnostics - report.never_executed_count;
        report.coverage_score = report.total_diagnostics > 0 ?
            static_cast<double>(executed) / report.total_diagnostics : 1.0;
        
        // Generate findings and recommendations
        generateFindings(report);
        
        return report;
    }

private:
    Config m_config;
    std::atomic<bool> m_enabled;
    mutable std::shared_mutex m_mutex;
    
    std::map<std::string, DiagnosticSummary> m_diagnostics;
    std::deque<DiagnosticExecution> m_executions;
    
    void generateFindings(DiagnosticQualityReport& report) const {
        // Finding: Many diagnostics never execute
        if (report.never_executed_count > report.total_diagnostics * 0.3) {
            report.findings.push_back(
                std::to_string(report.never_executed_count) + " of " +
                std::to_string(report.total_diagnostics) + " diagnostics (" +
                std::to_string(static_cast<int>((report.never_executed_count * 100.0) / report.total_diagnostics)) +
                "%) have never executed");
            report.recommendations.push_back(
                "Review unreachable diagnostics - may indicate dead code or incorrect assumptions about control flow");
        }
        
        // Finding: Broken diagnostics
        if (report.broken_count > 0) {
            report.findings.push_back(
                std::to_string(report.broken_count) + " diagnostics are failing consistently");
            report.recommendations.push_back(
                "Investigate failing diagnostics - they may indicate real bugs or outdated assertions");
        }
        
        // Finding: Subsystem-specific issues
        for (const auto& [sub, stats] : report.by_subsystem) {
            if (stats.second > 5) {  // More than 5 problematic in one subsystem
                report.findings.push_back(
                    "Subsystem " + subsystemToString(sub) + " has " +
                    std::to_string(stats.second) + " problematic diagnostics");
            }
        }
        
        // General recommendations
        if (report.coverage_score < 0.7) {
            report.recommendations.insert(report.recommendations.begin(),
                "Overall diagnostic coverage is low (" +
                std::to_string(static_cast<int>(report.coverage_score * 100)) +
                "%) - consider adding reachability markers to verify code paths");
        }
    }
    
    static std::string getCurrentThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
};

} // namespace diagnostics
} // namespace prosper_debug
