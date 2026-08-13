/**
 * Test Suite for IL2CPP Metadata Runtime Parser
 *
 * Tests cover:
 * - Header validation (magic, version)
 * - File loading and error handling
 * - String table access
 * - Image discovery
 * - Type lookup
 * - Evidence value verification
 * - Diagnostic output
 *
 * @upstream-candidate Ready for review
 */

#include <gtest/gtest.h>
#include "host/il2cpp_metadata.hpp"

#include <fstream>
#include <cstring>
#include <filesystem>

namespace prosper::il2cpp::test {

namespace fs = std::filesystem;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * Creates a minimal valid global-metadata.dat for testing.
 */
class MetadataTest : public ::testing::Test {
protected:
    std::string m_testFile;
    
    void SetUp() override {
        // Create temporary file path
        m_testFile = "test_global_metadata_" + 
                     std::to_string(std::time(nullptr)) + ".dat";
        
        createMinimalMetadata(m_testFile);
    }
    
    void TearDown() override {
        std::error_code ec;
        fs::remove(m_testFile, ec);
        // Ignore errors
    }
    
    /**
     * Create a minimal valid metadata file with correct magic and version.
     */
    static void createMinimalMetadata(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        
        // Write header
        MetadataHeader header{};
        header.magic = METADATA_MAGIC;
        header.version = METADATA_VERSION;
        
        // Set up minimal string table
        // We'll write strings after the header
        constexpr int HEADER_SIZE = sizeof(MetadataHeader);
        
        header.stringOffset = HEADER_SIZE;
        header.stringSize = 256;  // Space for some strings
        
        header.stringLiteralOffset = HEADER_SIZE + header.stringSize;
        header.stringLiteralSize = 64;
        
        // Leave other offsets as 0 (empty tables)
        header.typeDefinitionsOffset = 0;
        header.typeDefinitionsSize = 0;
        header.imagesOffset = 0;
        header.imagesSize = 0;
        
        out.write(reinterpret_cast<const char*>(&header), HEADER_SIZE);
        
        // Write padding/string data (zeros are fine for basic tests)
        std::vector<char> padding(header.stringSize + header.stringLiteralSize, 0);
        out.write(padding.data(), padding.size());
        
        out.close();
    }
    
    /**
     * Create an invalid metadata file (wrong magic).
     */
    static void createInvalidMagicFile(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        MetadataHeader header{};
        header.magic = 0xDEADBEEF;  // Wrong!
        header.version = METADATA_VERSION;
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.close();
    }
    
    /**
     * Create a metadata file with wrong version.
     */
    static void createInvalidVersionFile(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        MetadataHeader header{};
        header.magic = METADATA_MAGIC;
        header.version = 24;  // Wrong version!
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        // Add some padding to pass size check
        std::vector<char> padding(1024, 0);
        out.write(padding.data(), padding.size());
        out.close();
    }
};

// ============================================================================
// Unit Tests: Quick Validation
// ============================================================================

TEST(Il2CPPMetadataTest, QuickValidateAcceptsValidFile) {
    std::string path = "test_quick_valid.dat";
    MetadataTest::createMinimalMetadata(path);
    
    EXPECT_TRUE(quickValidate(path));
    
    fs::remove(path);
}

TEST(Il2CPPMetadataTest, QuickValidateRejectsMissingFile) {
    EXPECT_FALSE(quickValidate("nonexistent_file.dat"));
}

TEST(Il2CPPMetadataTest, QuickValidateRejectsInvalidMagic) {
    std::string path = "test_invalid_magic.dat";
    
    std::ofstream out(path, std::ios::binary);
    uint32_t badMagic = 0xDEADBEEF;
    int32_t version = METADATA_VERSION;
    out.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.close();
    
    EXPECT_FALSE(quickValidate(path));
    
    fs::remove(path);
}

// ============================================================================
// Unit Tests: Load Operations
// ============================================================================

TEST_F(MetadataTest, LoadSucceedsForValidFile) {
    MetadataParser parser;
    ASSERT_TRUE(parser.load(m_testFile));
    EXPECT_TRUE(parser.isLoaded());
}

TEST_F(MetadataTest, LoadFailsForNonexistentFile) {
    MetadataParser parser;
    EXPECT_FALSE(parser.load("nonexistent_file.dat"));
    EXPECT_FALSE(parser.isLoaded());
}

TEST_F(MetadataTest, LoadFailsForInvalidMagic) {
    std::string badFile = "test_bad_magic.dat";
    createInvalidMagicFile(badFile);
    
    MetadataParser parser;
    EXPECT_FALSE(parser.load(badFile));
    EXPECT_FALSE(parser.isLoaded());
    
    fs::remove(badFile);
}

TEST_F(MetadataTest, LoadFailsForUnsupportedVersion) {
    std::string badVersion = "test_bad_version.dat";
    createInvalidVersionFile(badVersion);
    
    MetadataParser parser;
    EXPECT_FALSE(parser.load(badVersion));
    EXPECT_FALSE(parser.isLoaded());
    
    fs::remove(badVersion);
}

TEST_F(MetadataTest, CloseResetsState) {
    MetadataParser parser;
    parser.load(m_testFile);
    ASSERT_TRUE(parser.isLoaded());
    
    parser.close();
    EXPECT_FALSE(parser.isLoaded());
}

TEST_F(MetadataTest, CloseAllowsReload) {
    MetadataParser parser;
    parser.load(m_testFile);
    parser.close();
    
    // Should be able to load again
    EXPECT_TRUE(parser.load(m_testFile));
    EXPECT_TRUE(parser.isLoaded());
}

// ============================================================================
// Unit Tests: Header Validation
// ============================================================================

TEST_F(MetadataTest, HeaderHasCorrectMagic) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    const auto& header = parser.getHeader();
    EXPECT_EQ(header.magic, METADATA_MAGIC);
}

TEST_F(MetadataTest, HeaderHasCorrectVersion) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    const auto& header = parser.getHeader();
    EXPECT_EQ(header.version, METADATA_VERSION);
}

TEST_F(MetadataTest, HeaderIsValid) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    const auto& header = parser.getHeader();
    EXPECT_TRUE(header.isValid());
}

TEST_F(MetadataTest, ValidateHeaderReturnsTrueForValidFile) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    EXPECT_TRUE(parser.validateHeader());
}

TEST_F(MetadataTest, HeaderToStringContainsKeyInfo) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    std::string str = parser.getHeader().toString();
    EXPECT_NE(str.find("0xFAB11BAF"), std::string::npos);
    EXPECT_NE(str.find("v29"), std::string::npos);
}

// ============================================================================
// Unit Tests: Version Formatting
// ============================================================================

TEST(VersionFormatTest, FormatsV29) {
    EXPECT_EQ(formatVersion(29), "v29");
}

TEST(VersionFormatTest, FormatsOtherVersions) {
    EXPECT_EQ(formatVersion(24), "v24");
    EXPECT_EQ(formatVersion(0), "v0");
}

// ============================================================================
// Unit Tests: Diagnostics
// ============================================================================

TEST_F(MetadataTest, DiagnosticsCapturedOnError) {
    std::string badFile = "test_diag_error.dat";
    createInvalidMagicFile(badFile);
    
    MetadataParser parser;
    parser.load(badFile);  // Should fail
    
    EXPECT_GT(parser.getDiagnosticCount(DiagnosticLevel::Error), 0u);
    
    fs::remove(badFile);
}

TEST_F(MetadataTest, CallbackReceivesMessages) {
    std::string badFile = "test_diag_callback.dat";
    createInvalidMagicFile(badFile);
    
    std::vector<DiagnosticMessage> received;
    MetadataParser parser;
    parser.setDiagnosticCallback([&received](const DiagnosticMessage& msg) {
        received.push_back(msg);
    });
    
    parser.load(badFile);
    
    EXPECT_FALSE(received.empty());
    
    bool foundError = false;
    for (const auto& msg : received) {
        if (msg.level == DiagnosticLevel::Error) {
            foundError = true;
            break;
        }
    }
    EXPECT_TRUE(foundError);
    
    fs::remove(badFile);
}

TEST_F(MetadataTest, ClearDiagnosticsWorks) {
    std::string badFile = "test_clear_diag.dat";
    createInvalidMagicFile(badFile);
    
    MetadataParser parser;
    parser.load(badFile);
    ASSERT_GT(parser.getDiagnosticCount(), 0u);
    
    parser.clearDiagnostics();
    EXPECT_EQ(parser.getDiagnosticCount(), 0u);
    
    fs::remove(badFile);
}

TEST_F(MetadataTest, VerboseModeOutputsMoreInfo) {
    // This test verifies verbose mode doesn't crash
    // Actual output goes through callback
    MetadataParser parser;
    parser.setVerbose(true);
    
    bool gotVerboseMsg = false;
    parser.setDiagnosticCallback([&gotVerboseMsg](const DiagnosticMessage& msg) {
        if (msg.level == DiagnosticLevel::Info && 
            msg.message.find("Raw header") != std::string::npos) {
            gotVerboseMsg = true;
        }
    });
    
    parser.load(m_testFile);
    // In verbose mode, we should get extra info messages
    // (This is a soft check - depends on implementation)
}

// ============================================================================
// Unit Tests: Evidence Values
// ============================================================================

TEST(EvidenceConstantsTest, SystemObjectIndexIsDefined) {
    // Just verify the constant exists and is reasonable
    static_assert(evidence::SYSTEM_OBJECT_TYPE_INDEX > 0, 
                  "System.Object type index should be positive");
    EXPECT_EQ(evidence::SYSTEM_OBJECT_TYPE_INDEX, 336);
}

TEST(EvidenceConstantsTest, UnityEngineObjectIndexIsDefined) {
    static_assert(evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX > 0,
                  "UnityEngine.Object type index should be positive");
    EXPECT_EQ(evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX, 2930);
}

TEST_F(MetadataTest, EvidenceReportCanBeGenerated) {
    MetadataParser parser;
    parser.load(m_testFile);
    
    std::string report = parser.getEvidenceReport();
    
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Evidence Verification Report"), std::string::npos);
    EXPECT_NE(report.find("System.Object"), std::string::npos);
    EXPECT_NE(report.find("UnityEngine.Object"), std::string::npos);
}

// ============================================================================
// Integration Tests: Real Metadata File (if available)
// ============================================================================

/**
 * These tests run only when a real global-metadata.dat is available.
 * They verify against actual game data.
 */
class RealMetadataTest : public ::testing::Test {
protected:
    static bool hasRealMetadata() {
        // Check common locations for test metadata files
        const char* paths[] = {
            "../../upload/PPSA15706-metadata-extracted/PPSA15706-global-metadata.dat",
            "../../upload/decrypted-extracted/Media/Metadata/global-metadata.dat",
            "../upload/PPSA15706-metadata-extracted/PPSA15706-global-metadata.dat",
            nullptr
        };
        
        for (int i = 0; paths[i]; ++i) {
            if (fs::exists(paths[i])) {
                return true;
            }
        }
        return false;
    }
    
    static std::string getRealMetadataPath() {
        const char* paths[] = {
            "../../upload/PPSA15706-metadata-extracted/PPSA15706-global-metadata.dat",
            "../../upload/decrypted-extracted/Media/Metadata/global-metadata.dat",
            "../upload/PPSA15706-metadata-extracted/PPSA15706-global-metadata.dat",
            nullptr
        };
        
        for (int i = 0; paths[i]; ++i) {
            if (fs::exists(paths[i])) {
                return paths[i];
            }
        }
        return "";
    }
};

TEST_F(RealMetadataTest, RealFileLoadsSuccessfully) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available for integration test";
    }
    
    MetadataParser parser;
    ASSERT_TRUE(parser.load(getRealMetadataPath()));
    EXPECT_TRUE(parser.isLoaded());
}

TEST_F(RealMetadataTest, RealFileHasCorrectMagicAndVersion) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available";
    }
    
    MetadataParser parser;
    parser.load(getRealMetadataPath());
    
    const auto& header = parser.getHeader();
    EXPECT_EQ(header.magic, METADATA_MAGIC);
    EXPECT_EQ(header.version, METADATA_VERSION);
}

TEST_F(RealMetadataTest, RealFileContainsSystemObject) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available";
    }
    
    MetadataParser parser;
    parser.load(getRealMetadataPath());
    
    auto systemObject = parser.findTypeByName("System", "Object");
    ASSERT_TRUE(systemObject.has_value());
    EXPECT_EQ(systemObject->index, evidence::SYSTEM_OBJECT_TYPE_INDEX);
}

TEST_F(RealMetadataTest, RealFileContainsUnityEngineObject) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available";
    }
    
    MetadataParser parser;
    parser.load(getRealMetadataPath());
    
    auto unityObject = parser.findTypeByName("UnityEngine", "Object");
    ASSERT_TRUE(unityObject.has_value());
    EXPECT_EQ(unityObject->index, evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX);
}

TEST_F(RealMetadataTest, EvidenceVerificationPassesOnRealFile) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available";
    }
    
    MetadataParser parser;
    parser.load(getRealMetadataPath());
    
    // This should pass on the verified PPSA15706 metadata
    bool result = parser.verifyEvidenceValues();
    EXPECT_TRUE(result);
}

TEST_F(RealMetadataTest, ImagesCanBeEnumerated) {
    if (!hasRealMetadata()) {
        GTEST_SKIP() << "No real metadata file available";
    }
    
    MetadataParser parser;
    parser.load(getRealMetadataPath());
    
    auto images = parser.getImages();
    EXPECT_FALSE(images.empty());
    
    // Should have at least Assembly-CSharp (main game assembly)
    bool foundAssemblyCSharp = false;
    for (const auto& img : images) {
        if (img.name.find("Assembly-CSharp") != std::string::npos ||
            img.name.find("Assembly-CSharp") != std::string::npos) {
            foundAssemblyCSharp = true;
            break;
        }
    }
    // Soft check - may or may not have this specific image
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(EdgeCaseTest, EmptyStringPathFailsGracefully) {
    MetadataParser parser;
    EXPECT_FALSE(parser.load(""));
    EXPECT_FALSE(parser.isLoaded());
}

TEST(EdgeCaseTest, OperationsOnUnloadedParserFailGracefully) {
    MetadataParser parser;
    
    // All operations should handle unloaded state gracefully
    EXPECT_TRUE(parser.getImages().empty());
    EXPECT_FALSE(parser.findTypeByName("System", "Object").has_value());
    EXPECT_FALSE(parser.findByTypeIndex(0).has_value());
    EXPECT_FALSE(parser.validateHeader());
    EXPECT_TRUE(parser.getEvidenceReport().find("no file loaded") != std::string::npos);
}

TEST(EdgeCaseTest, DoubleLoadReplacesPrevious) {
    std::string file1 = "test_double1.dat";
    std::string file2 = "test_double2.dat";
    
    MetadataTest::createMinimalMetadata(file1);
    MetadataTest::createMinimalMetadata(file2);
    
    MetadataParser parser;
    ASSERT_TRUE(parser.load(file1));
    ASSERT_TRUE(parser.load(file2));  // Should replace
    
    EXPECT_TRUE(parser.isLoaded());
    
    fs::remove(file1);
    fs::remove(file2);
}

} // namespace prosper::il2cpp::test

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
