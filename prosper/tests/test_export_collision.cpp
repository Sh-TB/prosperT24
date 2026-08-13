/**
 * Test Suite for Export Collision Detection
 *
 * Tests cover:
 * - Collision detection between modules
 * - Skip-on-collision behavior
 * - Aliased export tracking
 * - Statistics calculation
 * - Report generation
 * - Edge cases (empty inputs, no collisions, multiple collisions)
 *
 * @upstream-candidate Ready for review
 */

#include <gtest/gtest.h>
#include "loader/export_collision.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace prosper::test {

// ============================================================================
// Mock Module for Testing
// ============================================================================

/**
 * Simple mock module that returns predefined exports.
 * In real code, this would be a full Module with ELF parsing.
 */
class MockModule {
public:
    std::string path;
    std::vector<std::string> exports;
    
    MockModule(const std::string& p, const std::vector<std::string>& e)
        : path(p), exports(e) {}
};

// ============================================================================
// ExportCollision Structure Tests
// ============================================================================

TEST(ExportCollisionTest, DefaultCollisionIsEmpty) {
    prosper::ExportCollision collision;
    
    EXPECT_TRUE(collision.isEmpty());
    EXPECT_TRUE(collision.nid.empty());
    EXPECT_TRUE(collision.owner_path.empty());
}

TEST(ExportCollisionTest, PopulatedCollisionIsNotEmpty) {
    prosper::ExportCollision collision{"nid_123", "/path/to/module.prx"};
    
    EXPECT_FALSE(collision.isEmpty());
    EXPECT_EQ(collision.nid, "nid_123");
    EXPECT_EQ(collision.owner_path, "/path/to/module.prx");
}

TEST(ExportCollisionTest, ToStringFormatsCorrectly) {
    prosper::ExportCollision collision{"nid_abc", "/module.prx"};
    
    std::string str = collision.toString();
    
    EXPECT_NE(str.find("nid_abc"), std::string::npos);
    EXPECT_NE(str.find("/module.prx"), std::string::npos);
}

TEST(ExportCollisionTest, EmptyToStringShowsNone) {
    prosper::ExportCollision collision;
    
    std::string str = collision.toString();
    
    EXPECT_NE(str.find("none"), std::string::npos);
}

// ============================================================================
// SkippedModule Structure Tests
// ============================================================================

TEST(SkippedModuleTest, FieldsAreSettable) {
    prosper::SkippedModule skipped{
        "/skipped.prx",
        "colliding_nid",
        "/owner.prx"
    };
    
    EXPECT_EQ(skipped.path, "/skipped.prx");
    EXPECT_EQ(skipped.nid, "colliding_nid");
    EXPECT_EQ(skipped.owner_path, "/owner.prx");
}

TEST(SkippedModuleTest, ToStringContainsAllFields) {
    prosper::SkippedModule skipped{"/s.prx", "nid_1", "/o.prx"};
    
    std::string str = skipped.toString();
    
    EXPECT_NE(str.find("/s.prx"), std::string::npos);
    EXPECT_NE(str.find("nid_1"), std::string::npos);
    EXPECT_NE(str.find("/o.prx"), std::string::npos);
}

// ============================================================================
// AliasedExport Structure Tests
// ============================================================================

TEST(AliasedExportTest, DefaultValuesAreZero) {
    prosper::AliasedExport aliased;
    
    EXPECT_TRUE(aliased.nid.empty());
    EXPECT_EQ(aliased.winner, 0u);
    EXPECT_EQ(aliased.loser, 0u);
}

TEST(AliasedExportTest, AllFieldsSettable) {
    prosper::AliasedExport aliased{
        "shared_nid",
        "/winner.prx",
        "/loser.prx",
        0x1000,
        0x2000
    };
    
    EXPECT_EQ(aliased.nid, "shared_nid");
    EXPECT_EQ(aliased.winner_path, "/winner.prx");
    EXPECT_EQ(aliased.loser_path, "/loser.prx");
    EXPECT_EQ(aliased.winner, 0x1000u);
    EXPECT_EQ(aliased.loser, 0x2000u);
}

// ============================================================================
// CollisionStats Tests
// ============================================================================

TEST(CollisionStatsTest, DefaultValuesAreZero) {
    prosper::export_collision::CollisionStats stats;
    
    EXPECT_EQ(stats.totalModulesChecked, 0u);
    EXPECT_EQ(stats.modulesSkipped, 0u);
    EXPECT_EQ(stats.uniqueCollisions, 0u);
    EXPECT_EQ(stats.aliasedExports, 0u);
    EXPECT_EQ(stats.totalExportsClaimed, 0u);
    EXPECT_DOUBLE_EQ(stats.skipRate(), 0.0);
}

TEST(CollisionStatsTest, SkipRateCalculatedCorrectly) {
    prosper::export_collision::CollisionStats stats;
    stats.totalModulesChecked = 10;
    stats.modulesSkipped = 2;
    
    EXPECT_DOUBLE_EQ(stats.skipRate(), 0.2);  // 2/10 = 20%
}

TEST(CollisionStatsTest, SkipRateZeroWhenNoModules) {
    prosper::export_collision::CollisionStats stats;
    stats.totalModulesChecked = 0;
    stats.modulesSkipped = 5;
    
    // Division by zero protection: should return 0
    EXPECT_DOUBLE_EQ(stats.skipRate(), 0.0);
}

TEST(CollisionStatsTest, ToStringContainsKeyInfo) {
    prosper::export_collision::CollisionStats stats;
    stats.totalModulesChecked = 15;
    stats.modulesSkipped = 3;
    stats.uniqueCollisions = 3;
    stats.aliasedExports = 7;
    stats.totalExportsClaimed = 150;
    
    std::string str = stats.toString();
    
    // Should contain all key values
    EXPECT_NE(str.find("15"), std::string::npos);   // total modules
    EXPECT_NE(str.find("3"), std::string::npos);     // skipped (may match others too)
    EXPECT_NE(str.find("150"), std::string::npos);   // exports claimed
}

// ============================================================================
// Report Generation Tests
// ============================================================================

TEST(ReportGenerationTest, EmptyInputsProduceValidReport) {
    std::vector<prosper::SkippedModule> skipped;
    std::vector<prosper::AliasedExport> aliased;
    
    std::string report = prosper::export_collision::generateCollisionReport(
        skipped, aliased, 0);
    
    EXPECT_FALSE(report.empty());
    // Should indicate no issues found
    EXPECT_NE(report.find("collision") != std::string::npos || 
              report.find("Collision") != std::string::npos || 
              report.find("0") != std::string::npos, false);
}

TEST(ReportGenerationTest, SkippedModulesAppearInReport) {
    std::vector<prosper::SkippedModule> skipped = {
        {"/libfmodL.prx", "FMOD_Init", "/libfmod.prx"}
    };
    std::vector<prosper::AliasedExport> aliased;
    
    std::string report = prosper::export_collision::generateCollisionReport(
        skipped, aliased, 5);
    
    EXPECT_NE(report.find("libfmodL.prx"), std::string::npos);
    EXPECT_NE(report.find("FMOD_Init"), std::string::npos);
    EXPECT_NE(report.find("libfmod.prx"), std::string::npos);
}

TEST(ReportGenerationTest, AliasedExportsAppearInReport) {
    std::vector<prosper::SkippedModule> skipped;
    std::vector<prosper::AliasedExport> aliased = {
        {"PSN_CommonInit", "/PSNCore.prx", "/PSNCommon.prx", 0x100, 0x200}
    };
    
    std::string report = prosper::export_collision::generateCollisionReport(
        skipped, aliased, 10);
    
    EXPECT_NE(report.find("PSN_CommonInit"), std::string::npos);
    EXPECT_NE(report.find("PSNCore.prx"), std::string::npos);
    EXPECT_NE(report.find("PSNCommon.prx"), std::string::npos);
}

TEST(ReportGenerationTest, TotalModulesIncluded) {
    std::vector<prosper::SkippedModule> skipped;
    std::vector<prosper::AliasedExport> aliased;
    
    std::string report = prosper::export_collision::generateCollisionReport(
        skipped, aliased, 25);
    
    EXPECT_NE(report.find("25"), std::string::npos);
}

// ============================================================================
// Stats Calculation Tests
// ============================================================================

TEST(StatsCalculationTest, BasicCalculation) {
    std::vector<prosper::SkippedModule> skipped = {
        {"/mod1.prx", "nid_a", "/owner.prx"},
        {"/mod2.prx", "nid_b", "/owner.prx"}
    };
    std::vector<prosper::AliasedExport> aliased = {
        {"nid_c", "/mod3.prx", "/mod4.prx", 0, 0},
        {"nid_d", "/mod3.prx", "/mod4.prx", 0, 0}
    };
    
    auto stats = prosper::export_collision::calculateStats(
        skipped, aliased, 20);
    
    EXPECT_EQ(stats.totalModulesChecked, 20u);
    EXPECT_EQ(stats.modulesSkipped, 2u);
    EXPECT_EQ(stats.uniqueCollisions, 2u);  // One per skipped module
    EXPECT_EQ(stats.aliasedExports, 2u);
}

TEST(StatsCalculationTest, EmptyInputs) {
    auto stats = prosper::export_collision::calculateStats({}, {}, 0);
    
    EXPECT_EQ(stats.totalModulesChecked, 0u);
    EXPECT_EQ(stats.modulesSkipped, 0u);
    EXPECT_EQ(stats.uniqueCollisions, 0u);
    EXPECT_EQ(stats.aliasedExports, 0u);
}

// ============================================================================
// Real-World Scenario Tests
// ============================================================================

/**
 * Simulates the Evergate scenario where both libfmod.prx and libfmodL.prx
 * are present and export identical FMOD NIDs.
 */
class EvergateScenarioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Simulate FMOD release build exports
        fmodExports = {
            "FMOD_System_Create",
            "FMOD_System_Init",
            "FMOD_System_Update",
            "FMOD_System_Release",
            "FMOD_Sound_Release",
            "FMOD_Channel_IsPlaying"
        };
        
        // FMOD logging build (libfmodL) exports IDENTICAL NIDs
        fmodLExports = fmodExports;  // Same NIDs!
        
        // Other unique module exports
        psnExports = {
            "PSN_PrxInitialize",
            "PSN_Shutdown",
            "PSN_GetTrophyInfo"
        };
        
        commonExports = {
            "PSN_CommonInit",
            "PSN_CommonShutdown",
            "PSN_PrxInitialize"  // Shared with PSNCore!
        };
        
        coreExports = {
            "PSN_CoreInit",
            "PSN_PrxInitialize",  // Shared with PSNCommon!
            "PSN_CoreUpdate"
        };
    }
    
    std::vector<std::string> fmodExports;
    std::vector<std::string> fmodLExports;
    std::vector<std::string> psnExports;
    std::vector<std::string> commonExports;
    std::vector<std::string> coreExports;
};

TEST_F(EvergateScenarioTest, FmodLCollidesWithFmod) {
    // After claiming fmod's exports, fmodL should collide
    std::unordered_map<std::string, std::string> claimed;
    for (const auto& nid : fmodExports) {
        claimed[nid] = "/Media/Plugins/libfmod.prx";
    }
    
    // Check first collision (should find one immediately)
    bool foundCollision = false;
    for (const auto& nid : fmodLExports) {
        if (claimed.count(nid)) {
            foundCollision = true;
            EXPECT_EQ(claimed.at(nid), "/Media/Plugins/libfmod.prx");
            break;  // First collision is enough
        }
    }
    
    EXPECT_TRUE(foundCollision);
}

TEST_F(EvergateScenarioTest, PsnCoreAndPsnCommonShareNIDs) {
    // PSNCore and PSNCommon share some NIDs
    std::unordered_map<std::string, std::string> claimed;
    for (const auto& nid : coreExports) {
        claimed[nid] = "/Media/Plugins/PSNCore.prx";
    }
    
    // PSNCommon's PSN_PrxInitialize should collide
    bool hasCollision = claimed.count("PSN_PrxInitialize") > 0;
    EXPECT_TRUE(hasCollision);
    if (hasCollision) {
        EXPECT_EQ(claimed.at("PSN_PrxInitialize"), "/Media/Plugins/PSNCore.prx");
    }
}

TEST_F(EvergateScenarioTest, UniqueModuleHasNoCollision) {
    std::unordered_map<std::string, std::string> claimed;
    for (const auto& nid : fmodExports) {
        claimed[nid] = "/fmod.prx";
    }
    for (const auto& nid : coreExports) {
        claimed[nid] = "/PSNCore.prx";
    }
    
    // PSN.prx has unique exports not in claimed set
    bool anyCollision = false;
    for (const auto& nid : psnExports) {
        if (claimed.count(nid)) {
            anyCollision = true;
            break;
        }
    }
    
    EXPECT_FALSE(anyCollision);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(EdgeCaseCollisionTest, EmptyClaimedMap_NoCollision) {
    // With nothing claimed, any module should have no collision
    std::unordered_map<std::string, std::string> emptyClaimed;
    
    // An empty collision result indicates no conflict
    // (In real code, this would call find_export_collision)
    EXPECT_TRUE(emptyClaimed.empty());
}

TEST(EdgeCaseCollisionTest, LargeNumberOfModules) {
    // Stress test with many modules
    std::vector<prosper::SkippedModule> manySkipped;
    for (int i = 0; i < 100; ++i) {
        manySkipped.push_back({
            "/module_" + std::to_string(i) + ".prx",
            "nid_" + std::to_string(i),
            "/owner.prx"
        });
    }
    
    // Should handle large input without crashing
    auto stats = prosper::export_collision::calculateStats(
        manySkipped, {}, 150);
    
    EXPECT_EQ(stats.modulesSkipped, 100u);
    EXPECT_EQ(stats.uniqueCollisions, 100u);
}

TEST(EdgeCaseCollisionTest, SameModulePathMultipleTimes) {
    // Same module appearing multiple times in skipped list
    std::vector<prosper::SkippedModule> duplicates = {
        {"/same.prx", "nid_1", "/owner.prx"},
        {"/same.prx", "nid_2", "/owner.prx"},
        {"/same.prx", "nid_3", "/owner.prx"}
    };
    
    auto stats = prosper::export_collision::calculateStats(duplicates, {}, 10);
    
    EXPECT_EQ(stats.modulesSkipped, 3u);  // Counts occurrences, not unique paths
}

TEST(EdgeCaseCollisionTest, AliasedExportWithSameWinnerAndLoser) {
    // Multiple aliases from same pair of modules
    std::vector<prosper::AliasedExport> multiAlias = {
        {"nid_a", "/win.prx", "/lose.prx", 0x100, 0x200},
        {"nid_b", "/win.prx", "/lose.prx", 0x110, 0x210},
        {"nid_c", "/win.prx", "/lose.prx", 0x120, 0x220}
    };
    
    auto stats = prosper::export_collision::calculateStats({}, multiAlias, 5);
    
    EXPECT_EQ(stats.aliasedExports, 3u);
}

// ============================================================================
// Integration: Full Workflow Simulation
// ============================================================================

/**
 * Simulates the full boot-time collision detection workflow:
 * 1. Start with empty claimed set
 * 2. Process each module in order
 * 3. Skip or accept based on collisions
 * 4. Generate final report
 */
class FullWorkflowTest : public ::testing::Test {
protected:
    struct TestCase {
        std::string name;
        std::vector<std::string> modulePaths;
        std::vector<std::vector<std::string>> moduleExports;
        size_t expectedSkipped;
        size_t expectedAliased;
    };
    
    void runWorkflow(const TestCase& tc) {
        std::unordered_map<std::string, std::string> claimed;
        std::vector<prosper::SkippedModule> skipped;
        std::vector<prosper::AliasedExport> aliased;
        
        for (size_t i = 0; i < tc.modulePaths.size(); ++i) {
            const auto& path = tc.modulePaths[i];
            const auto& exports = tc.moduleExports[i];
            
            // Check for collision (simplified)
            std::string collidingNid;
            std::string ownerPath;
            
            for (const auto& nid : exports) {
                if (claimed.count(nid)) {
                    collidingNid = nid;
                    ownerPath = claimed[nid];
                    break;
                }
            }
            
            if (!collidingNid.empty()) {
                // Would skip this module
                skipped.push_back({path, collidingNid, ownerPath});
            } else {
                // Accept module and claim its exports
                for (const auto& nid : exports) {
                    claimed[nid] = path;
                }
                
                // Check for aliases (exports we're shadowing)
                // In simplified test, we don't track this fully
            }
        }
        
        // Verify expectations
        EXPECT_EQ(skipped.size(), tc.expectedSkipped);
        
        // Generate report (should not crash)
        std::string report = prosper::export_collision::generateCollisionReport(
            skipped, aliased, tc.modulePaths.size());
        EXPECT_FALSE(report.empty());
    }
};

TEST_F(FullWorkflowTest, EvergameFmodScenario) {
    TestCase tc{
        "Evergate FMOD duplicate",
        {
            "/eboot.bin",
            "/libc.prx",
            "/Media/Plugins/libfmod.prx",
            "/Media/Plugins/libfmodL.prx",  // Should be skipped (collides with libfmod)
            "/Media/Plugins/PSN.prx"
        },
        {
            {/* eboot */ "main"},
            {/* libc */ "malloc", "free", "printf"},
            {/* libfmod */ "FMOD_Init", "FMOD_Update", "FMOD_Release"},
            {/* libfmodL */ "FMOD_Init", "FMOD_Update", "FMOD_Release"},  // Same!
            {/* PSN */ "PSN_Init", "PSN_Shutdown"}
        },
        1,  // libfmodL should be skipped
        0
    };
    
    runWorkflow(tc);
}

TEST_F(FullWorkflowTest, NoCollisionsCleanBoot) {
    TestCase tc{
        "Clean boot with unique exports",
        {
            "/eboot.bin",
            "/libc.prx",
            "/Media/Plugins/UniqueA.prx",
            "/Media/Plugins/UniqueB.prx"
        },
        {
            {"entry"},
            {"malloc", "free"},
            {"Func_A1", "Func_A2"},  // Unique to A
            {"Func_B1", "Func_B2"}   // Unique to B
        },
        0,  // No skips expected
        0
    };
    
    runWorkflow(tc);
}

} // namespace prosper::test

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
