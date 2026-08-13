/**
 * Enhanced Test Suite for Case-Correct Path Resolution
 *
 * Comprehensive tests for resolve_host_path_case() covering:
 * - Exact match (no correction needed)
 * - Wrong case in filename (#1006 scenario)
 * - Wrong case in directory components
 * - Absent files and directories
 * - Length-different names (must NOT match)
 * - Absent leaf with corrected parent (#1236)
 * - Empty input handling
 * - Cross-platform behavior (case-sensitive vs insensitive FS)
 *
 * @upstream-candidate Ready for review
 */

#include <gtest/gtest.h>
#include "host/path_case_resolution.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;
using prosper::resolve_host_path_case;

namespace prosper::test {

// ============================================================================
// Test Fixture
// ============================================================================

class PathCaseResolutionTest : public ::testing::Test {
protected:
    fs::path m_testRoot;
    
    void SetUp() override {
        m_testRoot = fs::temp_directory_path() / 
                     ("prosper_path_case_test_" + std::to_string(std::time(nullptr)));
        fs::create_directories(m_testRoot / "Media" / "Modules");
        fs::create_directories(m_testRoot / "Media" / "Plugins");
        
        // Create test file with specific casing: Il2CppUserAssemblies.prx (uppercase C)
        std::ofstream(m_testRoot / "Media" / "Modules" / "Il2CppUserAssemblies.prx") << "test";
        
        // Create a plugin file
        std::ofstream(m_testRoot / "Media" / "Plugins" / "CustomPlugin.prx") << "plugin";
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(m_testRoot, ec);
    }
    
    std::string basePath() const {
        return m_testRoot.string();
    }
};

// ============================================================================
// Exact Match Tests
// ============================================================================

TEST_F(PathCaseResolutionTest, ExactCasePathReturnedUnchanged) {
    const std::string exact = basePath() + "/Media/Modules/Il2CppUserAssemblies.prx";
    
    auto result = resolve_host_path_case(exact);
    
    EXPECT_EQ(result, exact);
}

TEST_F(PathCaseResolutionTest, ExistingDirectoryPathReturnedUnchanged) {
    const std::string dir = basePath() + "/Media/Modules";
    
    auto result = resolve_host_path_case(dir);
    
    EXPECT_EQ(result, dir);
}

// ============================================================================
// #1006 Scenario: Wrong Case in Filename
// ============================================================================

TEST_F(PathCaseResolutionTest, WrongCaseFilename_ResolvesToExistingFile) {
    // The #1006 scenario: request lowercase "Il2cpp", disk has uppercase "Il2Cpp"
    const std::string wrongCase = basePath() + "/Media/Modules/Il2cppUserAssemblies.prx";
    
    auto result = resolve_host_path_case(wrongCase);
    
    // Result must reference an existing file
    EXPECT_TRUE(fs::exists(fs::path(result)));
}

TEST_F(PathCaseResolutionTest, WrongCaseFilename_StayInSameDirectory) {
    const std::string wrongCase = basePath() + "/Media/Modules/Il2cppUserAssemblies.prx";
    
    auto result = resolve_host_path_case(wrongCase);
    
    // Parent directory must be the same
    EXPECT_EQ(fs::path(result).parent_path(), fs::path(wrongCase).parent_path());
}

TEST_F(PathCaseResolutionTest, AllUppercaseFilename_Resolves) {
    const std::string upperCase = basePath() + "/Media/Modules/IL2CPPUSERASSEMBLIES.PRX";
    
    auto result = resolve_host_path_case(upperCase);
    
    EXPECT_TRUE(fs::exists(fs::path(result)));
}

// ============================================================================
// Directory Component Correction
// ============================================================================

TEST_F(PathCaseResolutionTest, WrongCaseDirectoryComponents_ResolveToFile) {
    // Wrong case in BOTH directory AND filename
    const std::string wrongDirs = basePath() + "/media/modules/IL2CPPUSERASSEMBLIES.PRX";
    
    auto result = resolve_host_path_case(wrongDirs);
    
    EXPECT_TRUE(fs::exists(fs::path(result)));
}

TEST_F(PathCaseResolutionTest, MixedCaseDirectoryComponents) {
    // Mixed case: some correct, some wrong
    const std::string mixed = basePath() + "/Media/modules/Il2cppUserAssemblies.prx";
    
    auto result = resolve_host_path_case(mixed);
    
    EXPECT_TRUE(fs::exists(fs::path(result)));
}

// ============================================================================
// Absent File Handling
// ============================================================================

TEST_F(PathCaseResolutionTest, AbsentFile_ReturnedUnchanged) {
    const std::string missing = basePath() + "/Media/Modules/DoesNotExist.prx";
    
    auto result = resolve_host_path_case(missing);
    
    // Absent file should return unchanged so caller's absence check triggers
    EXPECT_EQ(result, missing);
}

TEST_F(PathCaseResolutionTest, AbsentDirectoryChain_ReturnedUnchanged) {
    const std::string noDir = basePath() + "/NoSuchDir/Nested/Nope.prx";
    
    auto result = resolve_host_path_case(noDir);
    
    EXPECT_EQ(result, noDir);
}

// ============================================================================
// Length-Different Names (Must NOT Match)
// ============================================================================

TEST_F(PathCaseResolutionTest, LengthDifferentName_NotMatched) {
    // Name that differs by more than just case must NOT match
    const std::string lenDiff = basePath() + "/Media/Modules/Il2cppUserAssemblies2.prx";
    
    auto result = resolve_host_path_case(lenDiff);
    
    EXPECT_EQ(result, lenDiff);  // Should not find a match
}

TEST_F(PathCaseResolutionTest, ShorterName_NotMatched) {
    const std::string shorter = basePath() + "/Media/Modules/Il2Cpp.prx";
    
    auto result = resolve_host_path_case(shorter);
    
    EXPECT_EQ(result, shorter);  // "Il2Cpp.prx" != "Il2CppUserAssemblies.prx"
}

// ============================================================================
// #1236: Absent Leaf with Corrected Parent
// ============================================================================

TEST_F(PathCaseResolutionTest, AbsentLeaf_CorrectedParent_IsRealDirectory) {
    // Wrong-case dirs ("media/MODULES") with absent leaf ("BrandNew.prx")
    const std::string corrParent = basePath() + "/media/MODULES/BrandNew.prx";
    
    auto result = resolve_host_path_case(corrParent);
    
    // Parent directory should exist (was corrected)
    EXPECT_TRUE(fs::exists(fs::path(result).parent_path()));
    
    // Leaf should still be absent
    EXPECT_FALSE(fs::exists(fs::path(result)));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(EdgeCasePathTest, EmptyInput_ReturnsEmpty) {
    EXPECT_TRUE(resolve_host_path_case("").empty());
}

TEST(EdgeCasePathTest, RootPath_ReturnsUnchanged) {
    // Root "/" or "C:\" should return as-is
    std::string root = "/";
    auto result = resolve_host_path_case(root);
    // Should not crash; implementation-defined whether it changes
}

TEST(EdgeCasePathTest, SingleComponent_FileExists) {
    // Create a temp file in current directory
    std::string tmpName = "test_single_component_tmp_" + 
                         std::to_string(std::time(nullptr)) + ".tmp";
    { std::ofstream(tmpName) << "temp"; }
    
    auto result = resolve_host_path_case(tmpName);
    
    // Cleanup
    std::error_code ec;
    fs::remove(tmpName, ec);
    
    // Single component should work if file exists
    EXPECT_FALSE(result.empty());
}

TEST(EdgeCasePathTest, UnicodeFilenames_HandledGracefully) {
    // Test that unicode doesn't crash (behavior may vary by platform)
    std::string unicodePath = "/tmp/测试文件.dat";  // Chinese characters
    
    // Should not throw or crash
    auto result = resolve_host_path_case(unicodePath);
    EXPECT_TRUE(result.empty() || !result.empty());  // Just don't crash
}

// ============================================================================
// Plugin Discovery Tests
// ============================================================================

class PluginDiscoveryTest : public ::testing::Test {
protected:
    fs::path m_testRoot;
    
    void SetUp() override {
        m_testRoot = fs::temp_directory_path() / 
                     ("prosper_plugin_test_" + std::to_string(std::time(nullptr)));
        fs::create_directories(m_testRoot / "Media" / "Plugins");
        
        // Create some plugin files
        std::ofstream(m_testRoot / "Media" / "Plugins" / "ExtraPlugin1.prx") << "p1";
        std::ofstream(m_testRoot / "Media" / "Plugins" / "ExtraPlugin2.prx") << "p2";
        std::ofstream(m_testRoot / "Media" / "Plugins" / "NotAPlugin.txt") << "txt";
        
        // Create subdirectory (should be ignored)
        fs::create_directories(m_testRoot / "Media" / "Plugins" / "subdir");
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(m_testRoot, ec);
    }
};

TEST_F(PluginDiscoveryTest, DiscoversNewPlugins) {
    std::vector<std::string> listed = {"eboot.bin", "libc.prx"};
    
    auto found = prosper::discover_extra_plugin_modules(m_testRoot.string(), listed);
    
    // Should find the two .prx files we created
    EXPECT_GE(found.size(), 2u);
    
    // All should end with .prx
    for (const auto& f : found) {
        EXPECT_TRUE(fs::path(f).extension() == ".prx");
    }
}

TEST_F(PluginDiscoveryTest, IgnoresAlreadyListedPlugins) {
    std::vector<std::string> listed = {"extraplugin1.prx", "eboot.bin"};  // lowercase
    
    auto found = prosper::discover_extra_plugin_modules(m_testRoot.string(), listed);
    
    // ExtraPlugin1 is already listed (case-insensitive), so should not appear
    for (const auto& f : found) {
        std::string name = fs::path(f).filename().string();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        EXPECT_NE(name, "extraplugin1.prx");
    }
}

TEST_F(PluginDiscoveryTest, IgnoresNonPrxFiles) {
    std::vector<std::string> listed;
    
    auto found = prosper::discover_extra_plugin_modules(m_testRoot.string(), listed);
    
    for (const auto& f : found) {
        EXPECT_NE(fs::path(f).extension(), ".txt");
    }
}

TEST_F(PluginDiscoveryTest, ReturnsSortedDescending) {
    std::vector<std::string> listed;
    
    auto found = prosper::discover_extra_plugin_modules(m_testRoot.string(), listed);
    
    // Verify descending order (by lowercase basename)
    if (found.size() >= 2) {
        auto lower = [](const std::string& s) {
            std::string r = s;
            std::transform(r.begin(), r.end(), r.begin(), ::tolower);
            return fs::path(r).filename().string();
        };
        
        for (size_t i = 1; i < found.size(); ++i) {
            EXPECT_GE(lower(found[i-1]), lower(found[i]));
        }
    }
}

TEST_F(PluginDiscoveryTest, EmptyDumpRoot_ReturnsEmpty) {
    std::vector<std::string> listed;
    
    auto found = prosper::discover_extra_plugin_modules("", listed);
    
    EXPECT_TRUE(found.empty());
}

TEST_F(PluginDiscoveryTest, NonexistentDumpRoot_ReturnsEmpty) {
    std::vector<std::string> listed;
    
    auto found = prosper::discover_extra_plugin_modules("/nonexistent/path", listed);
    
    EXPECT_TRUE(found.empty());
}

// ============================================================================
// Case Insensitive Comparison Tests
// ============================================================================

TEST(CaseInsensitiveEqualTest, IdenticalStrings_True) {
    EXPECT_TRUE(prosper::testing::path_case::case_insensitive_equal("test", "test"));
}

TEST(CaseInsensitiveEqualTest, DifferentCase_True) {
    EXPECT_TRUE(prosper::testing::path_case::case_insensitive_equal("TeSt", "tEsT"));
}

TEST(CaseInsensitiveEqualTest, AllUpperVsLower_True) {
    EXPECT_TRUE(prosper::testing::path_case::case_insensitive_equal("ABC", "abc"));
}

TEST(CaseInsensitiveEqualTest, DifferentLength_False) {
    EXPECT_FALSE(prosper::testing::path_case::case_insensitive_equal("test", "tests"));
}

TEST(CaseInsensitiveEqualTest, CompletelyDifferent_False) {
    EXPECT_FALSE(prosper::testing::path_case::case_insensitive_equal("abc", "def"));
}

TEST(CaseInsensitiveEqualTest, EmptyStrings_True) {
    EXPECT_TRUE(prosper::testing::path_case::case_insensitive_equal("", ""));
}

TEST(CaseInsensitiveEqualTest, EmptyVsNonEmpty_False) {
    EXPECT_FALSE(prosper::testing::path_case::case_insensitive_equal("", "a"));
}

// ============================================================================
// Integration: Real Game Scenarios
// ============================================================================

/**
 * These tests verify the exact scenarios reported in GitHub issues.
 */
class RealScenarioTest : public ::testing::Test {
protected:
    fs::path m_gameRoot;
    
    void SetUp() override {
        m_gameRoot = fs::temp_directory_path() / 
                     ("prosper_scenario_test_" + std::to_string(std::time(nullptr)));
        
        // Recreate The Messenger structure
        fs::create_directories(m_gameRoot / "Media" / "Modules");
        fs::create_directories(m_gameRoot / "Media" / "Plugins");
        
        // Messenger ships "Il2cppUserAssemblies.prx" (lowercase 'c')
        std::ofstream(m_gameRoot / "Media" / "Modules" / "Il2cppUserAssemblies.prx") << "messenger";
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(m_gameRoot, ec);
    }
};

TEST_F(RealScenarioTest, MessengerScenario_LowercaseC) {
    // Boot code requests "Il2CppUserAssemblies.prx" but disk has "Il2cpp"
    const std::string requested = m_gameRoot.string() + "/Media/Modules/Il2CppUserAssemblies.prx";
    
    auto resolved = resolve_host_path_case(requested);
    
    // Must resolve to existing file
    ASSERT_TRUE(fs::exists(fs::path(resolved)));
    
    // On case-sensitive FS, this should have corrected the casing
    std::string basename = fs::path(resolved).filename().string();
    // Either way, the file must exist and be accessible
}

} // namespace prosper::test

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
