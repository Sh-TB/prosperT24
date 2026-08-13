/**
 * IL2CPP Metadata Runtime Parser for Prosper PS4 Emulator
 *
 * This module provides runtime parsing and validation of Unity IL2CPP
 * global-metadata.dat files used by PS4 games built with IL2CPP.
 *
 * Key capabilities:
 * - Global metadata header validation (magic 0xFAB11BAF, version v29)
 * - Image and type discovery
 * - Type lookup by name and index
 * - Diagnostic output for debugging
 *
 * Evidence values (verified from PPSA15706):
 *   Magic:        0xFAB11BAF
 *   Version:      29 (v29)
 *   System.Object:    type index 336
 *   UnityEngine.Object: type index 2930
 *
 * @upstream-candidate Ready for review
 * @author Prosper Team
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <functional>

namespace prosper {
namespace il2cpp {

// ============================================================================
// Constants
// ============================================================================

/// Global metadata magic number for IL2CPP global-format
constexpr uint32_t METADATA_MAGIC = 0xFAB11BAF;

/// Supported metadata version (v29)
constexpr int32_t METADATA_VERSION = 29;

/// Known evidence values from verified game (PPSA15706)
namespace evidence {
    /// System.Object type index in global-metadata.dat
    constexpr uint32_t SYSTEM_OBJECT_TYPE_INDEX = 336;
    
    /// UnityEngine.Object type index in global-metadata.dat  
    constexpr uint32_t UNITY_ENGINE_OBJECT_TYPE_INDEX = 2930;
} // namespace evidence

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Parsed global-metadata.dat header structure.
 *
 * Layout matches Unity's Il2CppGlobalMetadataHeader:
 * - Offset 0:  magic (uint32)
 * - Offset 4:  version (int32)
 * - Offset 8:  stringLiteralOffset (int32)
 * - ... additional fields
 */
struct MetadataHeader {
    uint32_t magic;              ///< Must be 0xFAB11BAF
    int32_t version;             ///< Must be 29
    int32_t stringLiteralOffset;
    int32_t stringLiteralSize;
    int32_t stringOffset;
    int32_t stringSize;
    int32_t eventsOffset;
    int32_t eventsSize;
    int32_t propertiesOffset;
    int32_t propertiesSize;
    int32_t methodsOffset;
    int32_t methodsSize;
    int32_t parameterDefaultValuesOffset;
    int32_t parameterDefaultValuesSize;
    int32_t fieldDefaultValuesOffset;
    int32_t fieldDefaultValuesSize;
    int32_t fieldAndParameterDefaultValueDataOffset;
    int32_t fieldAndParameterDefaultValueDataSize;
    int32_t fieldMarshaledSizesOffset;
    int32_t fieldMarshaledSizesSize;
    int32_t parametersOffset;
    int32_t parametersSize;
    int32_t fieldsOffset;
    int32_t fieldsSize;
    int32_t genericParametersOffset;
    int32_t genericParametersSize;
    int32_t genericParameterConstraintsOffset;
    int32_t genericParameterConstraintsSize;
    int32_t genericContainersOffset;
    int32_t genericContainersSize;
    int32_t nestedTypesOffset;
    int32_t nestedTypesSize;
    int32_t interfacesOffset;
    int32_t interfacesSize;
    int32_t vtableMethodsOffset;
    int32_t vtableMethodsSize;
    int32_t interfaceOffsetsOffset;
    int32_t interfaceOffsetsSize;
    int32_t typeDefinitionsOffset;
    int32_t typeDefinitionsSize;
    int32_t imagesOffset;
    int32_t imagesSize;
    int32_t assembliesOffset;
    int32_t assembliesSize;
    int32_t fieldRefsOffset;
    int32_t fieldRefsSize;
    int32_t referencedAssembliesOffset;
    int32_t referencedAssembliesSize;
    int32_t attributeDataOffset;
    int32_t attributeDataSize;
    int32_t attributeTypeRangeOffset;
    int32_t attributeTypeRangeSize;
    int32_t unresolvedVirtualCallParameterTypesOffset;
    int32_t unresolvedVirtualCallParameterTypesSize;
    int32_t unresolvedVirtualCallParameterRangesOffset;
    int32_t unresolvedVirtualCallParameterRangesSize;
    int32_t windowsRuntimeTypeNamesOffset;
    int32_t windowsRuntimeTypeNamesSize;
    int32_t windowsRuntimeStringsOffset;
    int32_t windowsRuntimeStringsSize;
    int32_t exportedTypeDefinitionsOffset;
    int32_t exportedTypeDefinitionsSize;

    bool isValid() const noexcept {
        return magic == METADATA_MAGIC && version == METADATA_VERSION;
    }

    std::string toString() const;
};

/**
 * Minimal image information from metadata.
 */
struct ImageInfo {
    uint32_t index;
    std::string name;
    int32_t typeStart;
    int32_t typeCount;
    
    std::string toString() const;
};

/**
 * Minimal type definition information.
 */
struct TypeInfo {
    uint32_t index;
    std::string name;
    std::string namespaceName;
    uint32_t imageIndex;         ///< Index into images table
    int32_t typeIndex;           ///< Type index within its image
    
    std::string fullName() const {
        if (namespaceName.empty()) return name;
        return namespaceName + "." + name;
    }
    
    std::string toString() const;
};

// ============================================================================
// Diagnostic Callback Types
// ============================================================================

enum class DiagnosticLevel {
    Info,
    Warning,
    Error
};

struct DiagnosticMessage {
    DiagnosticLevel level;
    std::string message;
    uint64_t offset;             ///< File offset if applicable
};

using DiagnosticCallback = std::function<void(const DiagnosticMessage&)>;

// ============================================================================
// Main Parser Class
// ============================================================================

/**
 * IL2CPP Global Metadata Parser
 *
 * Parses and validates Unity IL2CPP global-metadata.dat files.
 * Provides type lookup, image discovery, and diagnostic output.
 *
 * Usage:
 * @code
 *   il2cpp::MetadataParser parser;
 *   parser.setDiagnosticCallback([](const auto& msg) {
 *       std::cout << msg.message << "\n";
 *   });
 *   
 *   if (!parser.load("global-metadata.dat")) {
 *       // Handle error
 *   }
 *   
 *   auto systemObject = parser.findTypeByName("System", "Object");
 *   if (systemObject && systemObject->index == evidence::SYSTEM_OBJECT_TYPE_INDEX) {
 *       // Verified!
 *   }
 * @endcode
 */
class MetadataParser {
public:
    MetadataParser();
    ~MetadataParser();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    /**
     * Set diagnostic callback for parse messages.
     * Pass nullptr to disable diagnostics.
     */
    void setDiagnosticCallback(DiagnosticCallback callback);

    /**
     * Enable/disable verbose output (default: false).
     */
    void setVerbose(bool enabled);

    // -----------------------------------------------------------------------
    // Core Operations
    // -----------------------------------------------------------------------

    /**
     * Load and parse a global-metadata.dat file.
     *
     * @param path Path to the metadata file
     * @return true if loaded and validated successfully
     */
    bool load(const std::string& path);

    /**
     * Close current file and reset state.
     */
    void close();

    /**
     * Check if a file is currently loaded.
     */
    bool isLoaded() const noexcept { return m_loaded; }

    // -----------------------------------------------------------------------
    // Header Access
    // -----------------------------------------------------------------------

    /**
     * Get parsed header structure.
     * Only valid after successful load().
     */
    const MetadataHeader& getHeader() const noexcept { return m_header; }

    /**
     * Validate header against known constants.
     * Reports diagnostics via callback.
     */
    bool validateHeader();

    // -----------------------------------------------------------------------
    // String Table Access
    // -----------------------------------------------------------------------

    /**
     * Read string from string literal table at given offset.
     */
    std::optional<std::string> getStringLiteral(int32_t offset);

    /**
     * Read string from string table at given offset.
     */
    std::optional<std::string> getString(int32_t offset);

    // -----------------------------------------------------------------------
    // Image Discovery
    // -----------------------------------------------------------------------

    /**
     * Get all images from metadata.
     */
    std::vector<ImageInfo> getImages();

    /**
     * Find image by name (case-insensitive partial match).
     */
    std::optional<ImageInfo> findImage(const std::string& name);

    // -----------------------------------------------------------------------
    // Type Lookup
    // -----------------------------------------------------------------------

    /**
     * Get all type definitions.
     */
    std::vector<TypeInfo> getAllTypes();

    /**
     * Find type by namespace and name.
     */
    std::optional<TypeInfo> findTypeByName(
        const std::string& namespaceName,
        const std::string& typeName);

    /**
     * Find type by global type index.
     */
    std::optional<TypeInfo> findByTypeIndex(uint32_t typeIndex);

    // -----------------------------------------------------------------------
    // Evidence Verification
    // -----------------------------------------------------------------------

    /**
     * Verify known evidence values against loaded metadata.
     * Returns true if all checks pass.
     */
    bool verifyEvidenceValues();

    /**
     * Get evidence verification report as string.
     */
    std::string getEvidenceReport();

    // -----------------------------------------------------------------------
    // Diagnostics Summary
    // -----------------------------------------------------------------------

    /**
     * Get count of diagnostic messages by level.
     */
    size_t getDiagnosticCount(DiagnosticLevel level = DiagnosticLevel::Error) const;

    /**
     * Get all diagnostic messages since last clear.
     */
    std::vector<DiagnosticMessage> getDiagnostics() const;

    /**
     * Clear stored diagnostic messages.
     */
    void clearDiagnostics();

private:
    // Internal helpers
    bool readHeader();
    bool validateFileSize();
    void emit(DiagnosticLevel level, const std::string& msg, uint64_t offset = 0);
    bool readStringAt(int32_t tableOffset, int32_t tableSize, 
                      int32_t stringOffset, std::string& out);

    // State
    std::ifstream m_file;
    std::string m_path;
    MetadataHeader m_header{};
    bool m_loaded{false};
    bool m_verbose{false};
    DiagnosticCallback m_diagCallback;
    std::vector<DiagnosticMessage> m_diagnostics;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Quick validation of metadata file without full parse.
 *
 * @param path Path to check
 * @return true if file exists and has valid magic/version
 */
bool quickValidate(const std::string& path);

/**
 * Format metadata version as string (e.g., "v29").
 */
std::string formatVersion(int32_t version);

} // namespace il2cpp
} // namespace prosper
