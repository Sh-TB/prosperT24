/**
 * IL2CPP Metadata Runtime Parser - Implementation
 *
 * @see il2cpp_metadata.hpp for documentation
 * @upstream-candidate Ready for review
 */

#include "il2cpp_metadata.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace prosper {
namespace il2cpp {

// ============================================================================
// MetadataHeader Methods
// ============================================================================

std::string MetadataHeader::toString() const {
    std::ostringstream ss;
    ss << "MetadataHeader {\n"
       << "  magic: 0x" << std::hex << magic << std::dec 
       << (magic == METADATA_MAGIC ? " (valid)" : " (INVALID)") << "\n"
       << "  version: " << version << " (" << formatVersion(version) << ")"
       << (version == METADATA_VERSION ? " (valid)" : " (UNSUPPORTED)") << "\n"
       << "  stringLiteralOffset: " << stringLiteralOffset << "\n"
       << "  stringLiteralSize: " << stringLiteralSize << "\n"
       << "  stringOffset: " << stringOffset << "\n"
       << "  stringSize: " << stringSize << "\n"
       << "  typeDefinitionsOffset: " << typeDefinitionsOffset << "\n"
       << "  typeDefinitionsSize: " << typeDefinitionsSize << "\n"
       << "  imagesOffset: " << imagesOffset << "\n"
       << "  imagesSize: " << imagesSize << "\n"
       << "}";
    return ss.str();
}

// ============================================================================
// ImageInfo / TypeInfo Methods
// ============================================================================

std::string ImageInfo::toString() const {
    std::ostringstream ss;
    ss << "ImageInfo { index=" << index << ", name=\"" << name 
       << "\", types=[" << typeStart << ".." << (typeStart + typeCount - 1) 
       << "] }";
    return ss.str();
}

std::string TypeInfo::toString() const {
    std::ostringstream ss;
    ss << "TypeInfo { index=" << index << ", name=\"" << fullName()
       << "\", image=" << imageIndex << ", typeIndex=" << typeIndex << " }";
    return ss.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

bool quickValidate(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t magic;
    int32_t version;
    
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    return magic == METADATA_MAGIC && version == METADATA_VERSION;
}

std::string formatVersion(int32_t version) {
    return "v" + std::to_string(version);
}

// ============================================================================
// MetadataParser Implementation
// ============================================================================

MetadataParser::MetadataParser() = default;

MetadataParser::~MetadataParser() {
    close();
}

void MetadataParser::setDiagnosticCallback(DiagnosticCallback callback) {
    m_diagCallback = std::move(callback);
}

void MetadataParser::setVerbose(bool enabled) {
    m_verbose = enabled;
}

bool MetadataParser::load(const std::string& path) {
    close();
    
    m_path = path;
    m_file.open(path, std::ios::binary);
    
    if (!m_file.is_open()) {
        emit(DiagnosticLevel::Error, "Cannot open metadata file: " + path);
        return false;
    }
    
    emit(DiagnosticLevel::Info, "Loading metadata from: " + path);
    
    // Validate file size before reading header
    if (!validateFileSize()) {
        emit(DiagnosticLevel::Error, "File size validation failed");
        close();
        return false;
    }
    
    // Read and validate header
    if (!readHeader()) {
        emit(DiagnosticLevel::Error, "Failed to read/validate metadata header");
        close();
        return false;
    }
    
    // Run full header validation with diagnostics
    if (!validateHeader()) {
        emit(DiagnosticLevel::Error, "Header validation failed");
        close();
        return false;
    }
    
    m_loaded = true;
    emit(DiagnosticLevel::Info, "Metadata loaded successfully. Version: " + 
         formatVersion(m_header.version));
    
    return true;
}

void MetadataParser::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_loaded = false;
    m_path.clear();
    m_header = MetadataHeader{};
    // Note: Don't clear diagnostics on close - user may want to review them
}

bool MetadataParser::readHeader() {
    m_file.seekg(0, std::ios::beg);
    
    // Read the entire header in one operation for consistency
    // Header is fixed size based on Il2CppGlobalMetadataHeader layout
    constexpr size_t HEADER_SIZE = sizeof(MetadataHeader);
    
    // Verify we can read enough data
    m_file.seekg(0, std::ios::end);
    auto fileSize = m_file.tellg();
    if (fileSize < static_cast<std::streamoff>(HEADER_SIZE)) {
        emit(DiagnosticLevel::Error, "File too small for metadata header. "
             "Need " + std::to_string(HEADER_SIZE) + " bytes, got " + 
             std::to_string(fileSize));
        return false;
    }
    
    m_file.seekg(0, std::ios::beg);
    m_file.read(reinterpret_cast<char*>(&m_header), HEADER_SIZE);
    
    if (m_file.fail()) {
        emit(DiagnosticLevel::Error, "I/O error reading metadata header");
        return false;
    }
    
    if (m_verbose) {
        emit(DiagnosticLevel::Info, "Raw header read:\n" + m_header.toString());
    }
    
    return true;
}

bool MetadataParser::validateFileSize() {
    m_file.seekg(0, std::ios::end);
    auto size = m_file.tellg();
    
    // Minimum reasonable size: at least header + some data
    constexpr int64_t MIN_SIZE = 1024; // 1KB minimum
    
    if (size < MIN_SIZE) {
        emit(DiagnosticLevel::Error, "File suspiciously small: " + 
             std::to_string(size) + " bytes (minimum " + 
             std::to_string(MIN_SIZE) + ")");
        return false;
    }
    
    emit(DiagnosticLevel::Info, "File size: " + std::to_string(size) + " bytes");
    return true;
}

bool MetadataParser::validateHeader() const {
    bool valid = true;
    
    // Check magic number
    if (m_header.magic != METADATA_MAGIC) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%08X", m_header.magic);
        emit(DiagnosticLevel::Error, 
             "Invalid magic number: " + std::string(buf) + 
             " (expected 0xFAB11BAF)");
        valid = false;
    } else {
        emit(DiagnosticLevel::Info, "Magic number validated: 0xFAB11BAF");
    }
    
    // Check version
    if (m_header.version != METADATA_VERSION) {
        emit(DiagnosticLevel::Error, 
             "Unsupported version: " + formatVersion(m_header.version) +
             " (expected " + formatVersion(METADATA_VERSION) + ")");
        valid = false;
    } else {
        emit(DiagnosticLevel::Info, "Version validated: " + formatVersion(METADATA_VERSION));
    }
    
    // Sanity check offsets (must be non-negative and within file)
    m_file.seekg(0, std::ios::end);
    auto fileSize = m_file.tellg();
    
    auto checkOffset = [&](const char* name, int32_t offset, int32_t size) -> bool {
        if (offset < 0) {
            emit(DiagnosticLevel::Warning, 
                 std::string(name) + " offset is negative: " + std::to_string(offset));
            return false;
        }
        if (size < 0) {
            emit(DiagnosticLevel::Warning,
                 std::string(name) + " size is negative: " + std::to_string(size));
            return false;
        }
        if (offset + size > static_cast<int64_t>(fileSize)) {
            emit(DiagnosticLevel::Warning,
                 std::string(name) + " extends beyond file end: [" + 
                 std::to_string(offset) + ".." + std::to_string(offset + size) + 
                 "] > " + std::to_string(fileSize));
            return false;
        }
        return true;
    };
    
    checkOffset("stringLiteral", m_header.stringLiteralOffset, m_header.stringLiteralSize);
    checkOffset("string", m_header.stringOffset, m_header.stringSize);
    checkOffset("typeDefinitions", m_header.typeDefinitionsOffset, m_header.typeDefinitionsSize);
    checkOffset("images", m_header.imagesOffset, m_header.imagesSize);
    
    return valid;
}

std::optional<std::string> MetadataParser::getStringLiteral(int32_t offset) const {
    return readStringAt(m_header.stringLiteralOffset, m_header.stringLiteralSize, 
                        offset, /*out*/ std::string{});
}

std::optional<std::string> MetadataParser::getString(int32_t offset) const {
    std::string result;
    if (readStringAt(m_header.stringOffset, m_header.stringSize, offset, result)) {
        return result;
    }
    return std::nullopt;
}

bool MetadataParser::readStringAt(int32_t tableOffset, int32_t tableSize,
                                   int32_t stringOffset, std::string& out) const {
    if (!m_loaded || stringOffset < 0 || stringOffset >= tableSize) {
        return false;
    }
    
    auto absOffset = static_cast<int64_t>(tableOffset) + stringOffset;
    
    // Seek to string position
    m_file.clear();
    m_file.seekg(absOffset, std::ios::beg);
    
    if (m_file.fail()) {
        return false;
    }
    
    // Read null-terminated string
    out.clear();
    char ch;
    while (m_file.get(ch)) {
        if (ch == '\0') break;
        out += ch;
        
        // Safety limit to prevent reading garbage
        if (out.size() > 4096) break;
    }
    
    return !m_file.fail() && !out.empty();
}

std::vector<ImageInfo> MetadataParser::getImages() const {
    std::vector<ImageInfo> images;
    
    if (!m_loaded || m_header.imagesSize == 0) {
        return images;
    }
    
    // Image entry layout (simplified):
    // - nameIndex (int32): offset into string table
    // - typeStart (int32): starting type definition index
    // - typeCount (int32): number of type definitions
    
    constexpr int IMAGE_ENTRY_SIZE = 12; // 3 x int32
    
    int32_t count = m_header.imagesSize / IMAGE_ENTRY_SIZE;
    images.reserve(count);
    
    m_file.clear();
    m_file.seekg(m_header.imagesOffset, std::ios::beg);
    
    for (int32_t i = 0; i < count; ++i) {
        int32_t nameIndex, typeStart, typeCount;
        
        m_file.read(reinterpret_cast<char*>(&nameIndex), sizeof(nameIndex));
        m_file.read(reinterpret_cast<char*>(&typeStart), sizeof(typeStart));
        m_file.read(reinterpret_cast<char*>(&typeCount), sizeof(typeCount));
        
        if (m_file.fail()) break;
        
        ImageInfo img;
        img.index = static_cast<uint32_t>(i);
        img.typeStart = typeStart;
        img.typeCount = typeCount;
        
        // Get image name
        auto name = getString(nameIndex);
        img.name = name.value_or("<unknown>");
        
        images.push_back(img);
        
        if (m_verbose) {
            emit(DiagnosticLevel::Info, "Image: " + img.toString());
        }
    }
    
    return images;
}

std::optional<ImageInfo> MetadataParser::findImage(const std::string& name) const {
    auto images = getImages();
    
    // Case-insensitive search
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    for (const auto& img : images) {
        std::string lowerImg = img.name;
        std::transform(lowerImg.begin(), lowerImg.end(), lowerImg.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        if (lowerImg.find(lowerName) != std::string::npos) {
            return img;
        }
    }
    
    return std::nullopt;
}

std::vector<TypeInfo> MetadataParser::getAllTypes() const {
    std::vector<TypeInfo> types;
    
    if (!m_loaded || m_header.typeDefinitionsSize == 0) {
        return types;
    }
    
    // Type definition entry layout (simplified for v29):
    // - nameIndex (int32): offset into string table
    // - namespaceIndex (int32): offset into string table  
    // - imageIndex (int32): index into images table
    // ... plus other fields we don't need for basic lookup
    
    constexpr int TYPE_ENTRY_SIZE = 80; // Approximate for v29
    // Actual fields vary; we read what we need at known offsets
    
    m_file.clear();
    m_file.seekg(m_header.typeDefinitionsOffset, std::ios::beg);
    
    int32_t count = m_header.typeDefinitionsSize / TYPE_ENTRY_SIZE;
    types.reserve(count);
    
    for (int32_t i = 0; i < count; ++i) {
        auto basePos = m_file.tellg();
        
        TypeInfo info;
        info.index = static_cast<uint32_t>(i);
        
        // Read fields at known offsets (v29 layout)
        // Offset 0: nameIndex
        int32_t nameIndex;
        m_file.read(reinterpret_cast<char*>(&nameIndex), sizeof(nameIndex));
        
        // Offset 4: namespaceIndex
        int32_t namespaceIndex;
        m_file.read(reinterpret_cast<char*>(&namespaceIndex), sizeof(namespaceIndex));
        
        // Skip to imageIndex (offset varies by version)
        // For v29, it's typically after some flags/type fields
        m_file.seekg(basePos + static_cast<std::streamoff>(24), std::ios::beg);
        int32_t imageIndex;
        m_file.read(reinterpret_cast<char*>(&imageIndex), sizeof(imageIndex));
        
        if (m_file.fail()) break;
        
        // Resolve strings
        auto name = getString(nameIndex);
        auto ns = getString(namespaceIndex);
        
        info.name = name.value_or("<unknown>");
        info.namespaceName = ns.value_or("");
        info.imageIndex = static_cast<uint32_t>(imageIndex);
        info.typeIndex = i;
        
        types.push_back(info);
        
        // Seek to next entry
        m_file.seekg(basePos + static_cast<std::streamoff>(TYPE_ENTRY_SIZE), std::ios::beg);
    }
    
    return types;
}

std::optional<TypeInfo> MetadataParser::findTypeByName(
    const std::string& namespaceName,
    const std::string& typeName) const 
{
    // For efficiency in production, this should use hash tables.
    // This linear scan is acceptable for prototype/review.
    auto types = getAllTypes();
    
    for (const auto& type : types) {
        if (type.name == typeName && type.namespaceName == namespaceName) {
            return type;
        }
    }
    
    return std::nullopt;
}

std::optional<TypeInfo> MetadataParser::findByTypeIndex(uint32_t typeIndex) const {
    auto types = getAllTypes();
    
    for (const auto& type : types) {
        if (type.index == typeIndex) {
            return type;
        }
    }
    
    return std::nullopt;
}

bool MetadataParser::verifyEvidenceValues() const {
    bool allValid = true;
    
    // Check System.Object
    auto systemObject = findTypeByName("System", "Object");
    if (!systemObject) {
        emit(DiagnosticLevel::Error, 
             "Evidence FAILED: Cannot find System.Object in metadata");
        allValid = false;
    } else if (systemObject->index != evidence::SYSTEM_OBJECT_TYPE_INDEX) {
        emit(DiagnosticLevel::Error,
             "Evidence FAILED: System.Object has index " + 
             std::to_string(systemObject->index) + 
             " (expected " + std::to_string(evidence::SYSTEM_OBJECT_TYPE_INDEX) + ")");
        allValid = false;
    } else {
        emit(DiagnosticLevel::Info,
             "Evidence VERIFIED: System.Object at index " + 
             std::to_string(evidence::SYSTEM_OBJECT_TYPE_INDEX));
    }
    
    // Check UnityEngine.Object
    auto unityEngineObject = findTypeByName("UnityEngine", "Object");
    if (!unityEngineObject) {
        emit(DiagnosticLevel::Error,
             "Evidence FAILED: Cannot find UnityEngine.Object in metadata");
        allValid = false;
    } else if (unityEngineObject->index != evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX) {
        emit(DiagnosticLevel::Error,
             "Evidence FAILED: UnityEngine.Object has index " +
             std::to_string(unityEngineObject->index) +
             " (expected " + std::to_string(evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX) + ")");
        allValid = false;
    } else {
        emit(DiagnosticLevel::Info,
             "Evidence VERIFIED: UnityEngine.Object at index " +
             std::to_string(evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX));
    }
    
    return allValid;
}

std::string MetadataParser::getEvidenceReport() const {
    std::ostringstream report;
    
    report << "=== IL2CPP Evidence Verification Report ===\n\n";
    report << "File: " << (m_path.empty() ? "<not loaded>" : m_path) << "\n";
    report << "Loaded: " << (m_loaded ? "Yes" : "No") << "\n\n";
    
    if (!m_loaded) {
        report << "Cannot verify evidence: no file loaded.\n";
        return report.str();
    }
    
    report << "Header:\n";
    report << "  Magic: 0x" << std::hex << m_header.magic << std::dec << "\n";
    report << "  Version: " << formatVersion(m_header.version) << "\n\n";
    
    report << "Expected Evidence Values:\n";
    report << "  System.Object -> type index " << evidence::SYSTEM_OBJECT_TYPE_INDEX << "\n";
    report << "  UnityEngine.Object -> type index " << evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX << "\n\n";
    
    report << "Verification Results:\n";
    
    // System.Object
    auto systemObj = findTypeByName("System", "Object");
    if (systemObj && systemObj->index == evidence::SYSTEM_OBJECT_TYPE_INDEX) {
        report << "  [PASS] System.Object @ index " << systemObj->index << "\n";
    } else if (systemObj) {
        report << "  [FAIL] System.Object @ index " << systemObj->index 
               << " (expected " << evidence::SYSTEM_OBJECT_TYPE_INDEX << ")\n";
    } else {
        report << "  [FAIL] System.Object NOT FOUND\n";
    }
    
    // UnityEngine.Object
    auto unityObj = findTypeByName("UnityEngine", "Object");
    if (unityObj && unityObj->index == evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX) {
        report << "  [PASS] UnityEngine.Object @ index " << unityObj->index << "\n";
    } else if (unityObj) {
        report << "  [FAIL] UnityEngine.Object @ index " << unityObj->index
               << " (expected " << evidence::UNITY_ENGINE_OBJECT_TYPE_INDEX << ")\n";
    } else {
        report << "  [FAIL] UnityEngine.Object NOT FOUND\n";
    }
    
    report << "\nDiagnostic Summary:\n";
    report << "  Errors: " << getDiagnosticCount(DiagnosticLevel::Error) << "\n";
    report << "  Warnings: " << getDiagnosticCount(DiagnosticLevel::Warning) << "\n";
    report << "  Info: " << getDiagnosticCount(DiagnosticLevel::Info) << "\n";
    
    return report.str();
}

size_t MetadataParser::getDiagnosticCount(DiagnosticLevel level) const {
    if (level == DiagnosticLevel::Error) {
        return std::count_if(m_diagnostics.begin(), m_diagnostics.end(),
                            [](const auto& d) { return d.level == DiagnosticLevel::Error; });
    }
    return std::count_if(m_diagnostics.begin(), m_diagnostics.end(),
                        [&level](const auto& d) { return d.level == level; });
}

std::vector<DiagnosticMessage> MetadataParser::getDiagnostics() const {
    return m_diagnostics;
}

void MetadataParser::clearDiagnostics() {
    m_diagnostics.clear();
}

void MetadataParser::emit(DiagnosticLevel level, const std::string& msg, uint64_t offset) {
    DiagnosticMessage diag{level, msg, offset};
    m_diagnostics.push_back(diag);
    
    if (m_diagCallback) {
        m_diagCallback(diag);
    }
}

} // namespace il2cpp
} // namespace prosper
