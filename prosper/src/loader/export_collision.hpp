/**
 * Export Collision Detection for Prosper PS4 Emulator
 *
 * Detects and handles duplicate NID (Name/ID) exports across loaded modules.
 * Critical for preventing ambiguous symbol resolution when games ship multiple
 * builds of the same library (e.g., FMOD release + debug variants).
 *
 * Problem:
 * - Games may ship both libfmod.prx AND libfmodL.prx (release + logging builds)
 * - Both export identical NIDs but with different implementations
 * - Without detection: silent aliasing, unpredictable runtime behavior
 * - With detection: skip or report collisions, deterministic behavior
 *
 * Solution:
 * - Track all claimed NIDs in a global table
 * - Before linking each module, check for collisions
 * - Skip colliding modules OR report aliased exports
 * - First-definition-wins policy for global table
 *
 * @upstream-candidate Ready for review
 * @see loader/linker.cpp for primary integration
 * @author Prosper Team
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace prosper {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Represents a single export collision between two modules.
 *
 * When module B tries to export a NID that module A already owns,
 * this structure captures the conflict details.
 */
struct ExportCollision {
    std::string nid;           ///< The colliding NID (name/ID hash)
    std::string owner_path;    ///< Path to the module that already owns this NID
    
    bool isEmpty() const noexcept { return nid.empty(); }
    
    std::string toString() const {
        if (isEmpty()) return "ExportCollision{none}";
        return "ExportCollision{nid=" + nid + ", owner=" + owner_path + "}";
    }
};

/**
 * Record of a module that was skipped due to export collision.
 */
struct SkippedModule {
    std::string path;          ///< Path to the skipped module
    std::string nid;           ///< The NID that caused the collision
    std::string owner_path;    ///< Path to the module that already owned the NID
    
    std::string toString() const {
        return "SkippedModule{path=" + path + ", nid=" + nid + 
               ", owner=" + owner_path + "}";
    }
};

/**
 * Record of an aliased export (export that was accepted but shadows another).
 */
struct AliasedExport {
    std::string nid;           ///< The aliased NID
    std::string winner_path;   ///< Path to module whose export won (first definition)
    std::string loser_path;    ///< Path to module whose export was shadowed
    uint64_t winner = 0;       ///< Guest address of winning export
    uint64_t loser = 0;        ///< Guest address of losing (shadowed) export
    
    std::string toString() const {
        return "AliasedExport{nid=" + nid + ", winner=" + winner_path + 
               "(0x" + std::to_string(winner) + "), loser=" + loser_path +
               "(0x" + std::to_string(loser) + "))";
    }
};

// ============================================================================
// Core Functions
// ============================================================================

/**
 * Get all exported NIDs from a module.
 *
 * Returns defined (non-import), NID-bearing symbols with nonzero values.
 * These are the NIDs the module would contribute to the global export table.
 *
 * @param m  Module to query
 * @return   Vector of NID strings exported by this module
 */
std::vector<std::string> module_export_nids(const class Module& m);

/**
 * Find first export collision between a module and already-claimed NIDs.
 *
 * Scans the module's exports in file order, returning the first one
 * that's already in the `claimed` map. Deterministic behavior ensures
 * consistent results across runs.
 *
 * @param m       Module to check
 * @param claimed Map of NID -> owning module path (already-claimed exports)
 * @return        ExportCollision with details, or empty if no collision
 */
ExportCollision find_export_collision(
    const class Module& m,
    const std::unordered_map<std::string, std::string>& claimed);

// ============================================================================
// Diagnostic Utilities
// ============================================================================

namespace export_collision {

/**
 * Generate a human-readable collision report.
 *
 * @param skipped     Modules that were skipped due to collisions
 * @param aliased     Exports that were aliased (shadowed)
 * @param totalModules Total number of modules processed
 * @return            Formatted multi-line report string
 */
std::string generateCollisionReport(
    const std::vector<SkippedModule>& skipped,
    const std::vector<AliasedExport>& aliased,
    size_t totalModules);

/**
 * Summary statistics for collision detection.
 */
struct CollisionStats {
    size_t totalModulesChecked = 0;
    size_t modulesSkipped = 0;
    size_t uniqueCollisions = 0;
    size_t aliasedExports = 0;
    size_t totalExportsClaimed = 0;
    
    double skipRate() const {
        return totalModulesChecked > 0 
            ? static_cast<double>(modulesSkipped) / totalModulesChecked 
            : 0.0;
    }
    
    std::string toString() const;
};

/**
 * Calculate statistics from collision data.
 */
CollisionStats calculateStats(
    const std::vector<SkippedModule>& skipped,
    const std::vector<AliasedExport>& aliased,
    size_t totalModules);

} // namespace export_collision

} // namespace prosper
