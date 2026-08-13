/**
 * Export Collision Diagnostics Implementation
 *
 * @purpose
 * Implements the diagnostic analysis functions declared in export_collision_diagnostics.hpp.
 *
 * @design-notes
 * - Pure observer/aggregator logic—no loader modifications
 * - FAILURE != EMPTY semantics enforced throughout
 * - Deterministic output for same input
 * - No external dependencies beyond standard library
 *
 * @see export_collision_diagnostics.hpp for API documentation
 */

#include "export_collision_diagnostics.hpp"
#include <algorithm>
#include <set>

namespace prosper {
namespace export_collision_diagnostics {

// ============================================================================
// Core Analysis Implementation
// ============================================================================

DiagnosticResult analyze(
    const std::vector<std::tuple<std::string, std::string, std::string>>& skippedModules,
    const std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>& aliasedExports,
    size_t totalModules,
    size_t failedCount,
    size_t emptyExportCount)
{
    DiagnosticResult result;
    
    // === Input Validation & Status Determination ===
    // CRITICAL: Determine status BEFORE processing (FAILURE != EMPTY)
    
    if (totalModules == 0) {
        result.status = AnalysisStatus::EMPTY_INPUT;
        return result;  // Valid state, not an error
    }
    
    if (failedCount > totalModules) {
        // More failures than modules—data integrity issue
        result.status = AnalysisStatus::FAILED;
        result.errorMessage = "Failed module count exceeds total module count";
        result.stats.totalModulesChecked = totalModules;
        result.stats.failedModules = failedCount;  // Record as-is for debugging
        return result;
    }
    
    if (failedCount > 0 && failedCount < totalModules) {
        // Some modules failed but we got partial data
        result.status = AnalysisStatus::PARTIAL;
        result.warnings.push_back(
            std::to_string(failedCount) + " of " + 
            std::to_string(totalModules) + " modules failed analysis");
    } else if (failedCount == totalModules) {
        // Complete failure
        result.status = AnalysisStatus::FAILED;
        result.errorMessage = "All modules failed analysis";
        result.stats.totalModulesChecked = totalModules;
        result.stats.failedModules = failedCount;
        return result;
    } else {
        // No failures
        result.status = AnalysisStatus::COMPLETE;
    }
    
    // === Statistics Aggregation ===
    // Now populate stats with the actual collision data
    
    result.stats.totalModulesChecked = totalModules;
    result.stats.failedModules = failedCount;
    result.stats.emptyExportModules = emptyExportCount;
    
    // Calculate successful modules (total - failed)
    // Note: emptyExportModules are INCLUDED in successful—they succeeded, just had no exports
    result.stats.successfulModules = totalModules - failedCount;
    
    // Process skipped modules
    result.stats.skippedModules = skippedModules.size();
    
    // Process aliased exports
    result.stats.aliasedExports = aliasedExports.size();
    
    // === Collision Event Counting ===
    // Each skipped module = 1 collision event (the NID that caused the skip)
    // Each aliased export = 1 collision event
    
    result.stats.collisionEvents = skippedModules.size() + aliasedExports.size();
    
    // === Unique NID Tracking ===
    // Use a set to count distinct NIDs involved in collisions
    
    std::unordered_set<std::string> uniqueNIDs;
    
    // Collect NIDs from skipped modules
    for (const auto& [path, nid, owner] : skippedModules) {
        if (!nid.empty()) {
            uniqueNIDs.insert(nid);
        }
    }
    
    // Collect NIDs from aliased exports
    for (const auto& [nid, winnerPath, loserPath, winnerAddr, loserAddr] : aliasedExports) {
        if (!nid.empty()) {
            uniqueNIDs.insert(nid);
        }
    }
    
    result.stats.uniqueCollisionNIDs = uniqueNIDs.size();
    
    // === Module Impact Analysis ===
    // Track which modules are affected and how severely
    
    // Map: module path → impact summary
    std::unordered_map<std::string, ModuleImpact> impactMap;
    
    // Process skipped modules as impacts
    for (const auto& [path, nid, owner] : skippedModules) {
        ModuleImpact& impact = impactMap[path];
        impact.modulePath = path;
        impact.wasSkipped = true;
        impact.collisionCount++;
        if (!nid.empty() && 
            std::find(impact.collidingNIDs.begin(), impact.collidingNIDs.end(), nid) == impact.collidingNIDs.end()) {
            impact.collidingNIDs.push_back(nid);
        }
    }
    
    // Process aliased exports as impacts (both winner and loser affected)
    for (const auto& [nid, winnerPath, loserPath, winnerAddr, loserAddr] : aliasedExports) {
        // Loser module has aliased exports
        ModuleImpact& loserImpact = impactMap[loserPath];
        loserImpact.modulePath = loserPath;
        loserImpact.hasAliasedExports = true;
        loserImpact.collisionCount++;
        if (!nid.empty() &&
            std::find(loserImpact.collidingNIDs.begin(), loserImpact.collidingNIDs.end(), nid) == loserImpact.collidingNIDs.end()) {
            loserImpact.collidingNIDs.push_back(nid);
        }
        
        // Winner module is also "affected" (its export shadows another)
        ModuleImpact& winnerImpact = impactMap[winnerPath];
        winnerImpact.modulePath = winnerPath;
        winnerImpact.collisionCount++;
        if (!nid.empty() &&
            std::find(winnerImpact.collidingNIDs.begin(), winnerImpact.collidingNIDs.end(), nid) == winnerImpact.collidingNIDs.end()) {
            winnerImpact.collidingNIDs.push_back(nid);
        }
    }
    
    // Convert map to vector for the result
    for (auto& [path, impact] : impactMap) {
        result.affectedModules.push_back(std::move(impact));
    }
    
    // Sort by path for deterministic output
    std::sort(result.affectedModules.begin(), result.affectedModules.end(),
              [](const ModuleImpact& a, const ModuleImpact& b) {
                  return a.modulePath < b.modulePath;
              });
    
    // === Warnings Generation ===
    // Add contextual warnings based on the data
    
    if (result.stats.collisionEvents > 0 && result.stats.aliasedExports > 0) {
        result.warnings.push_back(
            "Aliased exports detected—behavior may be load-order dependent");
    }
    
    if (result.stats.emptyExportModules > result.stats.successfulModules / 2) {
        result.warnings.push_back(
            "More than half of analyzed modules have zero exports");
    }
    
    if (result.stats.failureRate() > 0.1) {  // >10% failure rate
        result.warnings.push_back(
            "High failure rate (" + 
            std::to_string(static_cast<int>(result.stats.failureRate() * 100)) + 
            "%)—check input data quality");
    }
    
    return result;
}

// ============================================================================
// Factory Functions
// ============================================================================

DiagnosticResult emptyResult() {
    DiagnosticResult result;
    result.status = AnalysisStatus::EMPTY_INPUT;
    // All stats remain at default (0) values
    // This is explicitly NOT a failure—it's an empty valid state
    return result;
}

DiagnosticResult failedResult(const std::string& errorMessage) {
    DiagnosticResult result;
    result.status = AnalysisStatus::FAILED;
    result.errorMessage = errorMessage;
    // Mark all stats as invalid/unknown by leaving at 0
    // Caller should check status before using stats
    return result;
}

} // namespace export_collision_diagnostics
} // namespace prosper
