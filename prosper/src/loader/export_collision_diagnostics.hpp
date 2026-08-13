/**
 * Export Collision Diagnostics Layer for Prosper PS4 Emulator
 *
 * @overview
 * This is a PURE DIAGNOSTICS LAYER that sits on top of the existing
 * export collision detection infrastructure implemented in #1670.
 *
 * @important
 * This file does NOT implement collision detection. That functionality
 * already exists in:
 *   - prosper/src/loader/linker.hpp (data structures)
 *   - prosper/src/loader/linker.cpp (implementation)
 *   - Issue #1635 → PR #1670 (original fix)
 *
 * @purpose
 * Provides observability and evidence reporting for collision data
 * collected by the existing loader infrastructure.
 *
 * @design-principles
 * 1. OBSERVER-ONLY: No loader behavior changes, no policy modifications
 * 2. FAILURE != EMPTY: Analysis failure is distinct from empty result set
 * 3. DUAL OUTPUT: Both human-readable and machine-readable (JSON) formats
 * 4. DETERMINISTIC: Same input always produces same output
 *
 * @usage
 * // After link_program() completes, use Program's aliased_exports
 * // and skipped_modules to generate diagnostic reports:
 *
 * prosper::export_collision_diagnostics::DiagnosticResult result =
 *     prosper::export_collision_diagnostics::analyze(program);
 *
 * std::string human_report = result.toText();
 * std::string json_report = result.toJson();
 *
 * @see prosper/src/loader/linker.hpp for source data structures
 * @see docs/EXPORT_COLLISION_DIAGNOSTICS.md for full documentation
 * @upstream-candidate Ready for review as diagnostics enhancement only
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace prosper {

// Forward declarations for upstream types (defined in linker.hpp)
// We REFERENCE these, we do NOT redefine them:
//   - ExportCollision { nid, owner_path }
//   - SkippedModule { path, nid, owner_path }  
//   - AliasedExport { nid, winner_path, loser_path, winner, loser }

namespace export_collision_diagnostics {

// ============================================================================
// Analysis Status — FAILURE != EMPTY Semantics
// ============================================================================

/**
 * Represents the outcome status of a diagnostic analysis run.
 *
 * CRITICAL: These states are MUTUALLY EXCLUSIVE and EXHAUSTIVE.
 *
 * FAILURE means the analysis could not complete (error, timeout, missing data).
 * EMPTY means the analysis completed successfully but found no data.
 * These are FUNDAMENTALLY different conditions.
 */
enum class AnalysisStatus {
    COMPLETE,        ///< Analysis finished successfully (may have 0+ findings)
    PARTIAL,         ///< Analysis completed with some modules unavailable
    FAILED,          ///< Analysis could not complete (error state)
    EMPTY_INPUT      ///< No modules provided for analysis (valid, not an error)
};

/**
 * Convert AnalysisStatus to human-readable string.
 */
inline const char* toString(AnalysisStatus status) {
    switch (status) {
        case AnalysisStatus::COMPLETE:    return "COMPLETE";
        case AnalysisStatus::PARTIAL:     return "PARTIAL";
        case AnalysisStatus::FAILED:      return "FAILED";
        case AnalysisStatus::EMPTY_INPUT: return "EMPTY_INPUT";
        default:                          return "UNKNOWN";
    }
}

// ============================================================================
// Collision Statistics — With FAILURE != EMPTY Semantics
// ============================================================================

/**
 * Comprehensive statistics for export collision diagnostic analysis.
 *
 * Each field tracks a DISTINCT metric. Do not conflate:
 * - collision_events: Total number of collision occurrences (can have duplicates)
 * - unique_collision_nids: Count of distinct NIDs involved in collisions
 * - failed_modules: Modules that could not be analyzed (ERROR state)
 * - empty_export_modules: Modules with zero exports (VALID state, not error)
 *
 * Example scenario:
 *   30 modules analyzed
 *   → 30 successful (no failures)
 *   → 18 have zero exports (valid empty sets)
 *   → 41 collision events (some NIDs collide multiple times)
 *   → 21 unique NIDs involved in those collisions
 *   → 0 failed (all modules were analyzable)
 */
struct CollisionStats {
    // Input metrics
    size_t totalModulesChecked = 0;       ///< Total modules submitted for analysis
    
    // Success/Failure/Empty tracking (MUTUALLY EXCLUSIVE per module)
    size_t successfulModules = 0;          ///< Modules successfully analyzed
    size_t failedModules = 0;              ///< Modules that FAILED analysis (error)
    size_t emptyExportModules = 0;         ///< Modules with valid EMPTY export sets
    
    // Collision metrics (only meaningful if successfulModules > 0)
    size_t collisionEvents = 0;            ///< Total collision occurrences (with duplicates)
    size_t uniqueCollisionNIDs = 0;        ///< Distinct NIDs involved in collisions
    size_t skippedModules = 0;             ///< Modules skipped due to collisions
    size_t aliasedExports = 0;             ///< Individual exports that were aliased
    
    // Derived metrics (calculated, not stored)
    
    /**
     * Calculate collision rate among non-empty modules.
     * Returns 0.0 if no modules had exports to analyze.
     */
    double collisionRate() const {
        size_t analyzable = successfulModules - emptyExportModules;
        return analyzable > 0 
            ? static_cast<double>(collisionEvents) / analyzable 
            : 0.0;
    }
    
    /**
     * Calculate failure rate.
     * High failure rate suggests data quality issues, not collision issues.
     */
    double failureRate() const {
        return totalModulesChecked > 0 
            ? static_cast<double>(failedModules) / totalModulesChecked 
            : 0.0;
    }
    
    /**
     * Calculate empty export rate.
     * Many PS4 modules legitimately have no exports (e.g., data-only modules).
     */
    double emptyExportRate() const {
        return totalModulesChecked > 0 
            ? static_cast<double>(emptyExportModules) / totalModulesChecked 
            : 0.0;
    }
    
    /**
     * Check if analysis produced any actionable findings.
     * Returns true if there are collisions to report.
     */
    bool hasCollisions() const noexcept {
        return collisionEvents > 0;
    }
    
    /**
     * Check if analysis completed without errors.
     * Note: COMPLETE can still have 0 collisions (clean system).
     */
    bool isSuccess() const noexcept {
        return failedModules == 0;
    }
    
    /**
     * Human-readable summary string.
     */
    std::string toString() const {
        std::ostringstream ss;
        ss << "CollisionStats{"
           << "total=" << totalModulesChecked
           << ", success=" << successfulModules
           << ", failed=" << failedModules
           << ", empty=" << emptyExportModules
           << ", events=" << collisionEvents
           << ", unique_nids=" << uniqueCollisionNIDs
           << ", skipped=" << skippedModules
           << ", aliased=" << aliasedExports
           << "}";
        return ss.str();
    }
    
    /**
     * Machine-readable JSON representation.
     * Uses stable field names for programmatic consumption.
     */
    std::string toJson() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"total_modules_checked\": " << totalModulesChecked << ",\n";
        ss << "  \"successful_modules\": " << successfulModules << ",\n";
        ss << "  \"failed_modules\": " << failedModules << ",\n";
        ss << "  \"empty_export_modules\": " << emptyExportModules << ",\n";
        ss << "  \"collision_events\": " << collisionEvents << ",\n";
        ss << "  \"unique_collision_nids\": " << uniqueCollisionNIDs << ",\n";
        ss << "  \"skipped_modules\": " << skippedModules << ",\n";
        ss << "  \"aliased_exports\": " << aliasedExports << ",\n";
        ss << "  \"collision_rate\": " << std::fixed << std::setprecision(4) << collisionRate() << ",\n";
        ss << "  \"failure_rate\": " << std::fixed << std::setprecision(4) << failureRate() << ",\n";
        ss << "  \"empty_export_rate\": " << std::fixed << std::setprecision(4) << emptyExportRate() << "\n";
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// Affected Module Tracking
// ============================================================================

/**
 * Summary of how a single module was affected by collisions.
 */
struct ModuleImpact {
    std::string modulePath;               ///< Path to the affected module
    size_t collisionCount = 0;            ///< Number of collisions this module is involved in
    std::vector<std::string> collidingNIDs; ///< Specific NIDs that collided
    bool wasSkipped = false;              ///< True if module was skipped entirely
    bool hasAliasedExports = false;       ///< True if some exports were shadowed
    
    std::string toJson() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"path\": \"" << modulePath << "\",\n";
        ss << "  \"collision_count\": " << collisionCount << ",\n";
        ss << "  \"was_skipped\": " << (wasSkipped ? "true" : "false") << ",\n";
        ss << "  \"has_aliased_exports\": " << (hasAliasedExports ? "true" : "false") << "\n";
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// Diagnostic Result — Complete Analysis Output
// ============================================================================

/**
 * Complete result of a collision diagnostic analysis.
 *
 * Contains all information needed for both human-readable reports
 * and machine-readable JSON output.
 *
 * Designed for:
 * - Console output (toText())
 * - JSON API consumption (toJson())
 * - CI artifact generation
 */
struct DiagnosticResult {
    AnalysisStatus status = AnalysisStatus::EMPTY_INPUT;
    CollisionStats stats;
    std::vector<ModuleImpact> affectedModules;
    std::vector<std::string> warnings;    ///< Non-fatal issues encountered
    std::string errorMessage;             ///< Set if status == FAILED
    
    /**
     * Generate human-readable text report.
     * Format suitable for console output or log files.
     */
    std::string toText() const {
        std::ostringstream report;
        
        report << "Export Collision Diagnostic Report\n";
        report << "====================================\n\n";
        
        // Status section
        report << "Status:\n";
        report << "  " << toString(status) << "\n\n";
        
        // Error handling (FAILURE != EMPTY)
        if (status == AnalysisStatus::FAILED && !errorMessage.empty()) {
            report << "Error:\n";
            report << "  " << errorMessage << "\n\n";
        }
        
        // Statistics section
        report << "Modules analyzed:\n";
        report << "  " << stats.totalModulesChecked << "\n\n";
        
        report << "Successful analysis:\n";
        report << "  " << stats.successfulModules << "\n\n";
        
        // FAILURE vs EMPTY distinction (CRITICAL)
        report << "Failed analysis:\n";
        report << "  " << stats.failedModules << "\n\n";
        
        report << "Modules with zero exports:\n";
        report << "  " << stats.emptyExportModules << "\n\n";
        
        // Only show collision stats if we actually analyzed something
        if (stats.successfulModules > 0 || status == AnalysisStatus::COMPLETE) {
            report << "Collision events:\n";
            report << "  " << stats.collisionEvents << "\n\n";
            
            report << "Unique conflicting NIDs:\n";
            report << "  " << stats.uniqueCollisionNIDs << "\n\n";
            
            report << "Skipped modules:\n";
            report << "  " << stats.skippedModules << "\n\n";
            
            report << "Aliased exports:\n";
            report << "  " << stats.aliasedExports << "\n\n";
            
            // Rates
            report << "Rates:\n";
            report << "  Collision rate: " << std::fixed << std::setprecision(2) 
                   << (stats.collisionRate() * 100) << "%\n";
            report << "  Failure rate: " << std::fixed << std::setprecision(2) 
                   << (stats.failureRate() * 100) << "%\n";
            report << "  Empty export rate: " << std::fixed << std::setprecision(2) 
                   << (stats.emptyExportRate() * 100) << "%\n\n";
        }
        
        // Affected modules detail
        if (!affectedModules.empty()) {
            report << "Affected modules:\n";
            for (const auto& impact : affectedModules) {
                report << "  " << impact.modulePath << "\n";
                report << "    Collisions: " << impact.collisionCount << "\n";
                if (impact.wasSkipped) report << "    Status: SKIPPED\n";
                if (impact.hasAliasedExports) report << "    Status: HAS_ALIASED_EXPORTS\n";
            }
            report << "\n";
        }
        
        // Warnings
        if (!warnings.empty()) {
            report << "Warnings:\n";
            for (const auto& warning : warnings) {
                report << "  ⚠ " << warning << "\n";
            }
            report << "\n";
        }
        
        // Severity assessment
        report << "Severity assessment:\n";
        if (status == AnalysisStatus::FAILED) {
            report << "  ❌ ANALYSIS FAILED - Results may be incomplete\n";
        } else if (stats.collisionEvents == 0) {
            report << "  ✅ CLEAN - No collisions detected\n";
        } else if (stats.aliasedExports == 0) {
            report << "  ⚠️ MODERATE - Collisions present but handled by skip logic\n";
        } else {
            report << "  🔶 ELEVATED - Aliased exports detected (load-order dependent)\n";
        }
        
        return report.str();
    }
    
    /**
     * Generate machine-readable JSON report.
     * Format suitable for:
     * - Programmatic parsing
     * - CI artifact storage
     * - Dashboard visualization
     * 
     * Field names are STABLE and DESCRIPTIVE for long-term compatibility.
     */
    std::string toJson() const {
        std::ostringstream json;
        
        json << "{\n";
        
        // Status
        json << "  \"status\": \"" << toString(status) << "\",\n";
        
        // Error (if any)
        if (status == AnalysisStatus::FAILED && !errorMessage.empty()) {
            json << "  \"error_message\": \"" << errorMessage << "\",\n";
        }
        
        // Statistics (using stats.toJson() for consistency)
        json << "  \"statistics\": " << stats.toJson() << ",\n";
        
        // Affected modules array
        json << "  \"affected_modules\": [\n";
        for (size_t i = 0; i < affectedModules.size(); ++i) {
            json << "    " << affectedModules[i].toJson();
            if (i < affectedModules.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Warnings array
        json << "  \"warnings\": [\n";
        for (size_t i = 0; i < warnings.size(); ++i) {
            json << "    \"" << warnings[i] << "\"";
            if (i < warnings.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ]\n";
        
        json << "}";
        return json.str();
    }
    
    /**
     * Check if this result indicates a clean (collision-free) system.
     * Note: EMPTY_INPUT is considered clean (nothing to analyze).
     */
    bool isClean() const noexcept {
        return status != AnalysisStatus::FAILED && stats.collisionEvents == 0;
    }
};

// ============================================================================
// Core Analysis Functions
// ============================================================================

/**
 * Analyze collision data from a completed link operation.
 *
 * This function aggregates raw collision data into structured statistics
 * and generates diagnostic reports. It does NOT perform collision detection
 * itself—that is the responsibility of the existing linker infrastructure.
 *
 * @param skippedModules  Modules skipped due to collisions (from Program::skipped_modules)
 * @param aliasedExports  Exports that were aliased (from Program::aliased_exports)
 * @param totalModules    Total number of modules that were processed
 * @param failedCount     Number of modules that couldn't be analyzed (0 if all succeeded)
 * @param emptyExportCount Number of modules with legitimately empty export sets
 * @return DiagnosticResult with complete analysis
 *
 * @note Implements FAILURE != EMPTY semantics:
 *   - failedCount > 0 → status includes failure indicators
 *   - emptyExportCount > 0 → tracked separately, not treated as failure
 */
DiagnosticResult analyze(
    const std::vector<std::tuple<std::string, std::string, std::string>>& skippedModules,
    const std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>& aliasedExports,
    size_t totalModules,
    size_t failedCount = 0,
    size_t emptyExportCount = 0);

/**
 * Create an empty/clean diagnostic result.
 * Useful for initialization or when no modules are available.
 */
DiagnosticResult emptyResult();

/**
 * Create a failed diagnostic result with error message.
 * Explicitly different from emptyResult()—this indicates ERROR, not absence of data.
 */
DiagnosticResult failedResult(const std::string& errorMessage);

} // namespace export_collision_diagnostics

} // namespace prosper
