/**
 * Export Collision Diagnostics — Standalone Test Suite
 *
 * @purpose
 * Tests for the diagnostics layer ONLY. Does NOT test collision detection
 * itself—that's covered by existing tests in test_plugin_autolink.cpp.
 *
 * @scope
 * - CollisionStats structure and calculations
 * - DiagnosticResult generation and formatting
 * - FAILURE != EMPTY semantics
 * - Report output (text and JSON)
 * - Edge cases and error conditions
 * - CLI-ready output stability
 *
 * @design-principles
 * 1. No duplication of upstream linker tests
 * 2. Focus on observability/reporting logic only
 * 3. Test all branches of FAILURE vs EMPTY logic
 * 4. Verify deterministic output
 * 5. Validate machine-readable format stability
 */

#include <gtest/gtest.h>
#include "loader/export_collision_diagnostics.hpp"

using namespace prosper::export_collision_diagnostics;

// ============================================================================
// Section 1: AnalysisStatus Enum Tests
// ============================================================================

TEST(AnalysisStatusTest, ToStringComplete) {
    EXPECT_STREQ(toString(AnalysisStatus::COMPLETE), "COMPLETE");
}

TEST(AnalysisStatusTest, ToStringPartial) {
    EXPECT_STREQ(toString(AnalysisStatus::PARTIAL), "PARTIAL");
}

TEST(AnalysisStatusTest, ToStringFailed) {
    EXPECT_STREQ(toString(AnalysisStatus::FAILED), "FAILED");
}

TEST(AnalysisStatusTest, ToStringEmptyInput) {
    EXPECT_STREQ(toString(AnalysisStatus::EMPTY_INPUT), "EMPTY_INPUT");
}

// ============================================================================
// Section 2: CollisionStats Structure Tests
// ============================================================================

TEST(CollisionStatsTest, DefaultConstruction) {
    CollisionStats stats;
    
    // All fields should default to 0
    EXPECT_EQ(stats.totalModulesChecked, 0u);
    EXPECT_EQ(stats.successfulModules, 0u);
    EXPECT_EQ(stats.failedModules, 0u);
    EXPECT_EQ(stats.emptyExportModules, 0u);
    EXPECT_EQ(stats.collisionEvents, 0u);
    EXPECT_EQ(stats.uniqueCollisionNIDs, 0u);
    EXPECT_EQ(stats.skippedModules, 0u);
    EXPECT_EQ(stats.aliasedExports, 0u);
}

TEST(CollisionStatsTest, HasCollisionsFalseWhenClean) {
    CollisionStats stats;
    stats.collisionEvents = 0;
    
    EXPECT_FALSE(stats.hasCollisions());
}

TEST(CollisionStatsTest, HasCollisionsTrueWhenPresent) {
    CollisionStats stats;
    stats.collisionEvents = 5;
    
    EXPECT_TRUE(stats.hasCollisions());
}

TEST(CollisionStatsTest, IsSuccessTrueWhenNoFailures) {
    CollisionStats stats;
    stats.totalModulesChecked = 10;
    stats.failedModules = 0;
    
    EXPECT_TRUE(stats.isSuccess());
}

TEST(CollisionStatsTest, IsSuccessFalseWhenFailuresPresent) {
    CollisionStats stats;
    stats.totalModulesChecked = 10;
    stats.failedModules = 2;
    
    EXPECT_FALSE(stats.isSuccess());
}

TEST(CollisionStatsTest, CollisionRateWithNoAnalyzable) {
    CollisionStats stats;
    stats.successfulModules = 5;  // All empty
    stats.emptyExportModules = 5;
    stats.collisionEvents = 0;
    
    // Should return 0.0 when no analyzable modules
    EXPECT_DOUBLE_EQ(stats.collisionRate(), 0.0);
}

TEST(CollisionStatsTest, CollisionRateNormalCase) {
    CollisionStats stats;
    stats.successfulModules = 30;  // 30 successful
    stats.emptyExportModules = 18; // 18 of those have no exports
    stats.collisionEvents = 41;   // 41 collision events
    
    // Analyzable = 30 - 18 = 12 modules
    // Rate = 41 / 12 ≈ 3.4167
    double expected_rate = 41.0 / 12.0;
    EXPECT_NEAR(stats.collisionRate(), expected_rate, 0.0001);
}

TEST(CollisionStatsTest, FailureRateCalculation) {
    CollisionStats stats;
    stats.totalModulesChecked = 100;
    stats.failedModules = 5;
    
    EXPECT_NEAR(stats.failureRate(), 0.05, 0.0001);
}

TEST(CollisionStatsTest, EmptyExportRateCalculation) {
    CollisionStats stats;
    stats.totalModulesChecked = 50;
    stats.emptyExportModules = 20;
    
    EXPECT_NEAR(stats.emptyExportRate(), 0.4, 0.0001);
}

TEST(CollisionStatsTest, ToStringFormat) {
    CollisionStats stats;
    stats.totalModulesChecked = 30;
    stats.successfulModules = 30;
    stats.failedModules = 0;
    stats.emptyExportModules = 18;
    stats.collisionEvents = 41;
    stats.uniqueCollisionNIDs = 21;
    
    std::string str = stats.toString();
    
    // Verify key fields appear in string
    EXPECT_NE(str.find("total=30"), std::string::npos);
    EXPECT_NE(str.find("success=30"), std::string::npos);
    EXPECT_NE(str.find("failed=0"), std::string::npos);
    EXPECT_NE(str.find("empty=18"), std::string::npos);
    EXPECT_NE(str.find("events=41"), std::string::npos);
    EXPECT_NE(str.find("unique_nids=21"), std::string::npos);
}

TEST(CollisionStatsTest, toJsonFormat) {
    CollisionStats stats;
    stats.totalModulesChecked = 10;
    stats.successfulModules = 8;
    stats.failedModules = 2;
    stats.emptyExportModules = 3;
    stats.collisionEvents = 5;
    stats.uniqueCollisionNIDs = 3;
    
    std::string json = stats.toJson();
    
    // Verify it's valid JSON structure with required fields
    EXPECT_NE(json.find("\"total_modules_checked\": 10"), std::string::npos);
    EXPECT_NE(json.find("\"successful_modules\": 8"), std::string::npos);
    EXPECT_NE(json.find("\"failed_modules\": 2"), std::string::npos);
    EXPECT_NE(json.find("\"empty_export_modules\": 3"), std::string::npos);
    EXPECT_NE(json.find("\"collision_events\": 5"), std::string::npos);
    EXPECT_NE(json.find("\"unique_collision_nids\": 3"), std::string::npos);
    EXPECT_NE(json.find("\"collision_rate\":"), std::string::npos);
    EXPECT_NE(json.find("\"failure_rate\":"), std::string::npos);
    EXPECT_NE(json.find("\"empty_export_rate\":"), std::string::npos);
}

// ============================================================================
// Section 3: FAILURE != EMPTY Semantics Tests (CRITICAL)
// ============================================================================

/**
 * These tests verify the fundamental design principle that analysis failure
 * is completely distinct from an empty result set.
 */

TEST(FailureVsEmptyTest, EmptyResultIsNotFailure) {
    DiagnosticResult result = emptyResult();
    
    // Empty input is NOT a failure
    EXPECT_EQ(result.status, AnalysisStatus::EMPTY_INPUT);
    EXPECT_NE(result.status, AnalysisStatus::FAILED);
    EXPECT_TRUE(result.isClean());  // Nothing to analyze = clean by definition
}

TEST(FailureVsEmptyTest, FailedResultIsNotEmpty) {
    DiagnosticResult result = failedResult("Test error message");
    
    // Failure is explicitly different from empty
    EXPECT_EQ(result.status, AnalysisStatus::FAILED);
    EXPECT_NE(result.status, AnalysisStatus::EMPTY_INPUT);
    EXPECT_FALSE(result.isClean());  // Failure means we can't trust results
}

TEST(FailureVsEmptyTest, CompleteWithZeroCollisionsIsClean) {
    // Create a complete analysis with no collisions
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 30, 0, 18);
    
    // This should be COMPLETE (not FAILED) and CLEAN (no collisions)
    EXPECT_EQ(result.status, AnalysisStatus::COMPLETE);
    EXPECT_TRUE(result.isClean());
    EXPECT_EQ(result.stats.totalModulesChecked, 30u);
    EXPECT_EQ(result.stats.successfulModules, 30u);  // All succeeded
    EXPECT_EQ(result.stats.failedModules, 0u);       // No failures
    EXPECT_EQ(result.stats.emptyExportModules, 18u);  // 18 legitimately empty
    EXPECT_EQ(result.stats.collisionEvents, 0u);     // But NO collisions
}

TEST(FailureVsEmptyTest, PartialFailureTracksBoth) {
    // Simulate partial failure scenario
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // 50 total, 5 failed, 20 empty exports
    DiagnosticResult result = analyze(skipped, aliased, 50, 5, 20);
    
    EXPECT_EQ(result.status, AnalysisStatus::PARTIAL);
    EXPECT_EQ(result.stats.totalModulesChecked, 50u);
    EXPECT_EQ(result.stats.failedModules, 5u);        // Explicit failures
    EXPECT_EQ(result.stats.emptyExportModules, 20u);  // Explicit empties
    EXPECT_EQ(result.stats.successfulModules, 45u);   // 50 - 5 failed
    
    // Verify they're tracked separately
    EXPECT_NE(result.stats.failedModules, result.stats.emptyExportModules);
}

TEST(FailureVsEmptyTest, TotalFailureIsErrorState) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // All modules failed
    DiagnosticResult result = analyze(skipped, aliased, 10, 10, 0);
    
    EXPECT_EQ(result.status, AnalysisStatus::FAILED);
    EXPECT_FALSE(result.isClean());  // Can't determine if clean
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Section 4: Core Analysis Function Tests
// ============================================================================

TEST(AnalyzeFunctionTest, BasicCollisionScenario) {
    // Simulate FMOD scenario: libfmod.prx collides with libfmodL.prx
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"libfmodL.prx", "NID_FMOD_INIT", "libfmod.prx"},
        {"libfmodL.prx", "NID_FMOD_UPDATE", "libfmod.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 15, 0, 5);
    
    EXPECT_EQ(result.status, AnalysisStatus::COMPLETE);
    EXPECT_EQ(result.stats.totalModulesChecked, 15u);
    EXPECT_EQ(result.stats.skippedModules, 2u);         // 2 skips
    EXPECT_EQ(result.stats.aliasedExports, 0u);          // No aliases
    EXPECT_EQ(result.stats.collisionEvents, 2u);         // 2 events total
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 2u);      // 2 unique NIDs
    EXPECT_TRUE(result.hasCollisions());
}

TEST(AnalyzeFunctionTest, AliasedExportsScenario) {
    // Scenario where exports are aliased (not skipped)
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{
        {"NID_PSN_GETUSERID", "PSNCore.prx", "PSNCommon.prx", 0x1000, 0x2000},
        {"NID_PSN_LOGIN", "PSNCore.prx", "PSNCommon.prx", 0x1100, 0x2100}
    };
    
    DiagnosticResult result = analyze(skipped, aliased, 8, 0, 2);
    
    EXPECT_EQ(result.status, AnalysisStatus::COMPLETE);
    EXPECT_EQ(result.stats.skippedModules, 0u);
    EXPECT_EQ(result.stats.aliasedExports, 2u);
    EXPECT_EQ(result.stats.collisionEvents, 2u);
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 2u);
}

TEST(AnalyzeFunctionTest, MixedScenario) {
    // Real-world mix of skips and aliases
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"libfmodL.prx", "NID_FMOD_INIT", "libfmod.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{
        {"NID_PSN_COMMON", "PSNCore.prx", "PSNCommon.prx", 0x1000, 0x2000}
    };
    
    DiagnosticResult result = analyze(skipped, aliased, 25, 0, 10);
    
    EXPECT_EQ(result.stats.collisionEvents, 2u);      // 1 skip + 1 alias
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 2u);  // 2 different NIDs
    EXPECT_EQ(result.stats.skippedModules, 1u);
    EXPECT_EQ(result.stats.aliasedExports, 1u);
}

TEST(AnalyzeFunctionTest, CleanSystem) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 40, 0, 22);
    
    EXPECT_TRUE(result.isClean());
    EXPECT_EQ(result.stats.collisionEvents, 0u);
    EXPECT_EQ(result.affectedModules.size(), 0u);
}

TEST(AnalyzeFunctionTest, DuplicateNIDCounting) {
    // Same NID appearing in multiple collisions should be counted once for unique
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"moduleB.prx", "NID_SHARED_001", "moduleA.prx"},
        {"moduleC.prx", "NID_SHARED_001", "moduleA.prx"},  // Same NID again
        {"moduleD.prx", "NID_SHARED_002", "moduleA.prx"}   // Different NID
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 20, 0, 5);
    
    EXPECT_EQ(result.stats.collisionEvents, 3u);      // 3 total events
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 2u);  // Only 2 unique NIDs
}

// ============================================================================
// Section 5: Module Impact Tracking Tests
// ============================================================================

TEST(ModuleImpactTest, SkippedModuleImpact) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"libfmodL.prx", "NID_FMOD_INIT", "libfmod.prx"},
        {"libfmodL.prx", "NID_FMOD_UPDATE", "libfmod.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 10, 0, 3);
    
    ASSERT_EQ(result.affectedModules.size(), 1u);
    
    const auto& impact = result.affectedModules[0];
    EXPECT_EQ(impact.modulePath, "libfmodL.prx");
    EXPECT_TRUE(impact.wasSkipped);
    EXPECT_FALSE(impact.hasAliasedExports);
    EXPECT_EQ(impact.collisionCount, 2u);
    EXPECT_EQ(impact.collidingNIDs.size(), 2u);
}

TEST(ModuleImpactTest, AliasedModuleImpact) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{
        {"NID_TEST", "winner.prx", "loser.prx", 0x1000, 0x2000}
    };
    
    DiagnosticResult result = analyze(skipped, aliased, 5, 0, 1);
    
    // Both winner and loser should be tracked as affected
    EXPECT_GE(result.affectedModules.size(), 1u);
    
    bool foundLoser = false;
    for (const auto& impact : result.affectedModules) {
        if (impact.modulePath == "loser.prx") {
            foundLoser = true;
            EXPECT_TRUE(impact.hasAliasedExports);
            EXPECT_FALSE(impact.wasSkipped);
        }
    }
    EXPECT_TRUE(foundLoser) << "Loser module should be in affected list";
}

// ============================================================================
// Section 6: Text Report Generation Tests
// ============================================================================

TEST(TextReportTest, CleanSystemReport) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 30, 0, 18);
    std::string report = result.toText();
    
    // Verify report contains key sections
    EXPECT_NE(report.find("Export Collision Diagnostic Report"), std::string::npos);
    EXPECT_NE(report.find("Status:"), std::string::npos);
    EXPECT_NE(report.find("COMPLETE"), std::string::npos);
    EXPECT_NE(report.find("Modules analyzed:"), std::string::npos);
    EXPECT_NE(report.find("30"), std::string::npos);
    EXPECT_NE(report.find("CLEAN"), std::string::npos);
    EXPECT_NE(report.find("Severity assessment:"), std::string::npos);
}

TEST(TextReportTest, CollisionReportContainsDetails) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"moduleB.prx", "NID_COLLISION", "moduleA.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 10, 0, 3);
    std::string report = result.toText();
    
    EXPECT_NE(report.find("Collision events:"), std::string::npos);
    EXPECT_NE(report.find("1"), std::string::npos);
    EXPECT_NE(report.find("Affected modules:"), std::string::npos);
    EXPECT_NE(report.find("moduleB.prx"), std::string::npos);
}

TEST(TextReportTest, FailureReportShowsError) {
    DiagnosticResult result = failedResult("Simulated failure");
    std::string report = result.toText();
    
    EXPECT_NE(report.find("FAILED"), std::string::npos);
    EXPECT_NE(report.find("Error:"), std::string::npos);
    EXPECT_NE(report.find("Simulated failure"), std::string::npos);
    EXPECT_NE(report.find("ANALYSIS FAILED"), std::string::npos);
}

TEST(TextReportTest, FailureVsEmptyDistinctionInReport) {
    // Empty case
    DiagnosticResult empty = emptyResult();
    std::string emptyReport = empty.toText();
    
    // Failure case
    DiagnosticResult failed = failedResult("error");
    std::string failedReport = failed.toText();
    
    // They should look fundamentally different
    EXPECT_NE(emptyReport.find("EMPTY_INPUT"), std::string::npos);
    EXPECT_NE(failedReport.find("FAILED"), std::string::npos);
    
    // Empty should not contain error section
    EXPECT_EQ(emptyReport.find("Error:"), std::string::npos);
    
    // Failed should contain error section
    EXPECT_NE(failedReport.find("Error:"), std::string::npos);
}

// ============================================================================
// Section 7: JSON Output Tests (CLI-Ready)
// ============================================================================

TEST(JsonOutputTest, ValidJsonStructure) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"moduleB.prx", "NID_COL", "moduleA.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 5, 0, 2);
    std::string json = result.toJson();
    
    // Verify JSON structure
    EXPECT_NE(json.find("{"), std::string::npos);
    EXPECT_NE(json.find("}"), std::string::npos);
    EXPECT_NE(json.find("\"status\":"), std::string::npos);
    EXPECT_NE(json.find("\"statistics\":"), std::string::npos);
    EXPECT_NE(json.find("\"affected_modules\":"), std::string::npos);
    EXPECT_NE(json.find("\"warnings\":"), std::string::npos);
}

TEST(JsonOutputTest, StableFieldNames) {
    CollisionStats stats;
    stats.totalModulesChecked = 42;
    std::string json = stats.toJson();
    
    // Verify stable field names for CLI/AI consumption
    EXPECT_NE(json.find("\"total_modules_checked\""), std::string::npos);
    EXPECT_NE(json.find("\"successful_modules\""), std::string::npos);
    EXPECT_NE(json.find("\"failed_modules\""), std::string::npos);
    EXPECT_NE(json.find("\"empty_export_modules\""), std::string::npos);
    EXPECT_NE(json.find("\"collision_events\""), std::string::npos);
    EXPECT_NE(json.find("\"unique_collision_nids\""), std::string::npos);
    EXPECT_NE(json.find("\"skipped_modules\""), std::string::npos);
    EXPECT_NE(json.find("\"aliased_exports\""), std::string::npos);
    // Derived metrics also present
    EXPECT_NE(json.find("\"collision_rate\""), std::string::npos);
    EXPECT_NE(json.find("\"failure_rate\""), std::string::npos);
    EXPECT_NE(json.find("\"empty_export_rate\""), std::string::npos);
}

TEST(JsonOutputTest, MachineReadableValues) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 100, 5, 30);
    std::string json = result.toJson();
    
    // Values should be machine-readable (not formatted with commas, etc.)
    EXPECT_NE(json.find("\"total_modules_checked\": 100"), std::string::npos);
    EXPECT_NE(json.find("\"successful_modules\": 95"), std::string::npos);  // 100-5
    EXPECT_NE(json.find("\"failed_modules\": 5"), std::string::npos);
    EXPECT_NE(json.find("\"empty_export_modules\": 30"), std::string::npos);
}

TEST(JsonOutputTest, ContainsStatusField) {
    DiagnosticResult result = emptyResult();
    std::string json = result.toJson();
    
    EXPECT_NE(json.find("\"status\": \"EMPTY_INPUT\""), std::string::npos);
}

TEST(JsonOutputTest, FailedJsonContainsError) {
    DiagnosticResult result = failedResult("test error");
    std::string json = result.toJson();
    
    EXPECT_NE(json.find("\"status\": \"FAILED\""), std::string::npos);
    EXPECT_NE(json.find("\"error_message\":"), std::string::npos);
    EXPECT_NE(json.find("test error"), std::string::npos);
}

// ============================================================================
// Section 8: Deterministic Output Tests
// ============================================================================

TEST(DeterministicOutputTest, SameInputProducesSameTextOutput) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"moduleB.prx", "NID_COL", "moduleA.prx"}
    };
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result1 = analyze(skipped, aliased, 10, 0, 3);
    DiagnosticResult result2 = analyze(skipped, aliased, 10, 0, 3);
    
    // Two analyses of same data should produce identical text output
    EXPECT_EQ(result1.toText(), result2.toText());
}

TEST(DeterministicOutputTest, SameInputProducesSameJsonOutput) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"moduleB.prx", "NID_COL", "moduleA.prx"}
    };
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result1 = analyze(skipped, aliased, 10, 0, 3);
    DiagnosticResult result2 = analyze(skipped, aliased, 10, 0, 3);
    
    // Two analyses of same data should produce identical JSON output
    EXPECT_EQ(result1.toJson(), result2.toJson());
}

TEST(DeterministicOutputTest, SortedAffectedModules) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"z_module.prx", "NID_Z", "a_module.prx"},
        {"a_module.prx", "NID_A", "first.prx"},
        {"m_module.prx", "NID_M", "middle.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 15, 0, 5);
    
    // Affected modules should be sorted by path for deterministic output
    if (result.affectedModules.size() >= 2) {
        for (size_t i = 1; i < result.affectedModules.size(); ++i) {
            EXPECT_LE(result.affectedModules[i-1].modulePath,
                      result.affectedModules[i].modulePath)
                << "Affected modules should be sorted alphabetically";
        }
    }
}

// ============================================================================
// Section 9: Edge Cases and Boundary Conditions
// ============================================================================

TEST(EdgeCaseTest, ZeroTotalModules) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 0, 0, 0);
    
    EXPECT_EQ(result.status, AnalysisStatus::EMPTY_INPUT);
    EXPECT_TRUE(result.isClean());
}

TEST(EdgeCaseTest, SingleModuleNoCollisions) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 1, 0, 1);
    
    EXPECT_EQ(result.status, AnalysisStatus::COMPLETE);
    EXPECT_EQ(result.stats.totalModulesChecked, 1u);
    EXPECT_EQ(result.stats.emptyExportModules, 1u);
    EXPECT_TRUE(result.isClean());
}

TEST(EdgeCaseTest, SingleModuleWithCollision) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"module.prx", "NID_ONLY", "other.prx"}
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 2, 0, 0);
    
    EXPECT_EQ(result.stats.collisionEvents, 1u);
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 1u);
}

TEST(EdgeCaseTest, ManyCollisionsFewUniqueNIDs) {
    // Scenario: many modules collide on few shared NIDs
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>;
    for (int i = 1; i <= 20; ++i) {
        skipped.push_back({"module_" + std::to_string(i) + ".prx", 
                          "NID_SHARED", "original.prx"});
    }
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    DiagnosticResult result = analyze(skipped, aliased, 25, 0, 4);
    
    EXPECT_EQ(result.stats.collisionEvents, 20u);     // 20 events
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 1u);  // Only 1 unique NID
}

TEST(EdgeCaseTest, EmptyNIDStringsHandledGracefully) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{
        {"module.prx", "", "owner.prx"}  // Empty NID string
    };
    
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // Should not crash, empty NID just doesn't count toward unique
    DiagnosticResult result = analyze(skipped, aliased, 3, 0, 1);
    
    EXPECT_EQ(result.stats.collisionEvents, 1u);      // Event still counted
    EXPECT_EQ(result.stats.uniqueCollisionNIDs, 0u);  // But not unique NID
}

TEST(EdgeCaseTest, DataIntegrityError_MoreFailuresThanTotal) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // Impossible state: more failures than total modules
    DiagnosticResult result = analyze(skipped, aliased, 5, 10, 0);
    
    EXPECT_EQ(result.status, AnalysisStatus::FAILED);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Section 10: Warning Generation Tests
// ============================================================================

TEST(WarningTest, AliasedExportsWarning) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{
        {"NID_X", "win.prx", "lose.prx", 0x1000, 0x2000}
    };
    
    DiagnosticResult result = analyze(skipped, aliased, 5, 0, 1);
    
    // Should generate warning about load-order dependency
    bool foundWarning = false;
    for (const auto& w : result.warnings) {
        if (w.find("load-order") != std::string::npos) {
            foundWarning = true;
            break;
        }
    }
    EXPECT_TRUE(foundWarning) << "Should warn about load-order dependent behavior";
}

TEST(WarningTest, HighEmptyExportRateWarning) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // More than half have empty exports
    DiagnosticResult result = analyze(skipped, aliased, 10, 0, 6);
    
    bool foundWarning = false;
    for (const auto& w : result.warnings) {
        if (w.find("zero exports") != std::string::npos) {
            foundWarning = true;
            break;
        }
    }
    EXPECT_TRUE(foundWarning) << "Should warn about high empty export rate";
}

TEST(WarningTest, HighFailureRateWarning) {
    auto skipped = std::vector<std::tuple<std::string, std::string, std::string>>{};
    auto aliased = std::vector<std::tuple<std::string, std::string, std::string, uint64_t, uint64_t>>{};
    
    // >10% failure rate triggers warning
    DiagnosticResult result = analyze(skipped, aliased, 100, 15, 30);
    
    bool foundWarning = false;
    for (const auto& w : result.warnings) {
        if (w.find("failure rate") != std::string::npos) {
            foundWarning = true;
            break;
        }
    }
    EXPECT_TRUE(foundWarning) << "Should warn about high failure rate";
}

// ============================================================================
// Main - Test Runner
// ============================================================================

} // anonymous namespace for test organization
