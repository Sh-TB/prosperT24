/**
 * Debug Intelligence Platform - Core Foundation Types
 * 
 * Based on analysis of 3000+ Prosper/SharpEmuT24 debugging investigations.
 * 
 * DESIGN PHILOSOPHY:
 * - Observer Only: Zero modifications to emulator behavior
 * - Evidence First: Every conclusion requires supporting data
 * - Root Cause Focus: Crash location ≠ corruption location
 * - Temporal Awareness: When matters as much as what
 * 
 * @version 2.0.0
 * @license MIT
 * 
 * GOLDEN RULES:
 * 1. No loader/HLE/GPU/CPU behavior changes
 * 2. No automatic fixes or hidden corrections
 * 3. Every diagnostic must prove: when, who, what evidence, rejected hypotheses
 * 4. Performance impact only when explicitly enabled
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <memory>
#include <array>
#include <cstdint>
#include <ctime>

namespace fs = std::filesystem;
namespace prosper_debug {

// ============================================================================
// Version & Metadata
// ============================================================================

constexpr const char* PLATFORM_VERSION = "2.0.0";
constexpr const char* PLATFORM_NAME = "Prosper Debug Intelligence";

// ============================================================================
// Timestamp Utilities
// ============================================================================

/**
 * High-resolution timestamp for precise event tracking
 */
class DebugTimestamp {
public:
    static DebugTimestamp now() {
        DebugTimestamp ts;
        ts.wall_clock = std::chrono::system_clock::now();
        ts.steady_clock = std::chrono::steady_clock::now();
        ts.frame_number = 0;  // Set by caller if in emulation context
        return ts;
    }
    
    static DebugTimestamp atFrame(uint64_t frame) {
        DebugTimestamp ts = now();
        ts.frame_number = frame;
        return ts;
    }
    
    /**
     * ISO 8601 formatted timestamp
     */
    std::string toISO8601() const {
        auto time_t = std::chrono::system_clock::to_time_t(wall_clock);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
    
    /**
     * File-friendly timestamp (no colons)
     */
    std::string toFileFriendly() const {
        auto time_t = std::chrono::system_clock::to_time_t(wall_clock);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
        
        // Add milliseconds for uniqueness
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            wall_clock.time_since_epoch()) % 1000;
        ss << "_" << ms.count();
        
        return ss.str();
    }
    
    /**
     * Microseconds since epoch (for sorting/comparison)
     */
    uint64_t toMicroseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            wall_clock.time_since_epoch()).count();
    }
    
    // Time sources
    std::chrono::system_clock::time_point wall_clock;
    std::chrono::steady_clock::time_point steady_clock;
    uint64_t frame_number{0};
    
    // Comparison operators
    inline bool operator<(const DebugTimestamp& other) const {
        return wall_clock < other.wall_clock;
    }
    
    inline bool operator>(const DebugTimestamp& other) const {
        return wall_clock > other.wall_clock;
    }
    
    inline bool operator==(const DebugTimestamp& other) const {
        return wall_clock == other.wall_clock;
    }
    
    inline bool operator>=(const DebugTimestamp& other) const {
        return wall_clock >= other.wall_clock;
    }
    
    inline bool operator<=(const DebugTimestamp& other) const {
        return wall_clock <= other.wall_clock;
    }
    
    inline bool operator!=(const DebugTimestamp& other) const {
        return wall_clock != other.wall_clock;
    }
};

// ============================================================================
// Subsystem Identification
// ============================================================================

/**
 * Emulator subsystems for source attribution
 */
enum class Subsystem : uint8_t {
    Unknown = 0,
    
    // Core systems
    CPU,
    Memory,
    GPU,
    Audio,
    Input,
    
    // Kernel layer
    Kernel,
    FileSystem,
    ThreadManager,
    Synchronization,
    
    // HLE layer
    HLE_LibKernel,
    HLE_LibGpu,
    HLE_LibAudio,
    HLE_LibNet,
    HLE_LibPng,
    HLE_LibJpeg,
    HLE_LibZlib,
    HLE_LibSsl,
    HLE_LibCrypto,
    HLE_Other,
    
    // Application layer
    GuestApplication,
    IL2CPP_Runtime,
    GameCode,
    
    // Diagnostic infrastructure
    DiagnosticSystem,
    TestFramework,
    
    // External
    HostSystem,
    FileSystemHost
};

/**
 * Convert subsystem enum to string
 */
inline std::string subsystemToString(Subsystem subs) {
    switch (subs) {
        case Subsystem::Unknown: return "Unknown";
        case Subsystem::CPU: return "CPU";
        case Subsystem::Memory: return "Memory";
        case Subsystem::GPU: return "GPU";
        case Subsystem::Audio: return "Audio";
        case Subsystem::Input: return "Input";
        case Subsystem::Kernel: return "Kernel";
        case Subsystem::FileSystem: return "FileSystem";
        case Subsystem::ThreadManager: return "ThreadManager";
        case Subsystem::Synchronization: return "Synchronization";
        case Subsystem::HLE_LibKernel: return "HLE.LibKernel";
        case Subsystem::HLE_LibGpu: return "HLE.LibGpu";
        case Subsystem::HLE_LibAudio: return "HLE.LibAudio";
        case Subsystem::HLE_LibNet: return "HLE.LibNet";
        case Subsystem::HLE_LibPng: return "HLE.LibPng";
        case Subsystem::HLE_LibJpeg: return "HLE.LibJpeg";
        case Subsystem::HLE_LibZlib: return "HLE.LibZlib";
        case Subsystem::HLE_LibSsl: return "HLE.LibSsl";
        case Subsystem::HLE_LibCrypto: return "HLE.LibCrypto";
        case Subsystem::HLE_Other: return "HLE.Other";
        case Subsystem::GuestApplication: return "GuestApplication";
        case Subsystem::IL2CPP_Runtime: return "IL2CPP.Runtime";
        case Subsystem::GameCode: return "GameCode";
        case Subsystem::DiagnosticSystem: return "DiagnosticSystem";
        case Subsystem::TestFramework: return "TestFramework";
        case Subsystem::HostSystem: return "HostSystem";
        case Subsystem::FileSystemHost: return "FileSystemHost";
    }
    return "Unknown";
}

/**
 * Parse subsystem from string
 */
inline Subsystem subsystemFromString(const std::string& str) {
    static const std::map<std::string, Subsystem> map = {
        {"CPU", Subsystem::CPU},
        {"Memory", Subsystem::Memory},
        {"GPU", Subsystem::GPU},
        {"Audio", Subsystem::Audio},
        {"Input", Subsystem::Input},
        {"Kernel", Subsystem::Kernel},
        {"FileSystem", Subsystem::FileSystem},
        {"ThreadManager", Subsystem::ThreadManager},
        {"Synchronization", Subsystem::Synchronization},
        {"HLE.LibKernel", Subsystem::HLE_LibKernel},
        {"HLE.LibGpu", Subsystem::HLE_LibGpu},
        {"HLE.LibAudio", Subsystem::HLE_LibAudio},
        {"HLE.LibNet", Subsystem::HLE_LibNet},
        {"HLE.LibPng", Subsystem::HLE_LibPng},
        {"HLE.LibJpeg", Subsystem::HLE_LibJpeg},
        {"HLE.LibZlib", Subsystem::HLE_LibZlib},
        {"HLE.LibSsl", Subsystem::HLE_LibSsl},
        {"HLE.LibCrypto", Subsystem::HLE_LibCrypto},
        {"HLE.Other", Subsystem::HLE_Other},
        {"GuestApplication", Subsystem::GuestApplication},
        {"IL2CPP.Runtime", Subsystem::IL2CPP_Runtime},
        {"GameCode", Subsystem::GameCode},
        {"DiagnosticSystem", Subsystem::DiagnosticSystem},
        {"TestFramework", Subsystem::TestFramework},
        {"HostSystem", Subsystem::HostSystem},
        {"FileSystemHost", Subsystem::FileSystemHost}
    };
    
    auto it = map.find(str);
    return it != map.end() ? it->second : Subsystem::Unknown;
}

// ============================================================================
// Severity Levels
// ============================================================================

enum class Severity : uint8_t {
    Info = 0,
    Warning,
    Error,
    Critical,
    Fatal
};

inline std::string severityToString(Severity sev) {
    switch (sev) {
        case Severity::Info: return "Info";
        case Severity::Warning: return "Warning";
        case Severity::Error: return "Error";
        case Severity::Critical: return "Critical";
        case Severity::Fatal: return "Fatal";
    }
    return "Info";
}

inline Severity severityFromString(const std::string& str) {
    if (str == "Warning") return Severity::Warning;
    if (str == "Error") return Severity::Error;
    if (str == "Critical") return Severity::Critical;
    if (str == "Fatal") return Severity::Fatal;
    return Severity::Info;
}

// ============================================================================
// Source Location - Who Caused This?
// ============================================================================

/**
 * Precise source attribution for any event
 */
struct SourceLocation {
    std::string file_name;
    int line_number{0};
    std::string function_name;
    std::string module_name;  // Library/module
    
    // For HLE calls
    std::string hle_function_name;
    int hle_function_id{-1};
    
    // For guest code
    uint64_t guest_address{0};
    std::string symbol_name;
    
    bool isValid() const {
        return !file_name.empty() || !function_name.empty() || 
               !hle_function_name.empty() || guest_address != 0;
    }
    
    std::string toString() const {
        std::stringstream ss;
        
        if (!hle_function_name.empty()) {
            ss << hle_function_name;
            if (!file_name.empty()) {
                ss << " at " << file_name;
                if (line_number > 0) {
                    ss << ":" << line_number;
                }
            }
        } else if (!function_name.empty()) {
            ss << function_name << "()";
            if (!file_name.empty()) {
                ss << " at " << file_name << ":" << line_number;
            }
        } else if (guest_address != 0) {
            ss << "0x" << std::hex << guest_address << std::dec;
            if (!symbol_name.empty()) {
                ss << " (" << symbol_name << ")";
            }
        } else {
            ss << "unknown location";
        }
        
        return ss.str();
    }
    
    /**
     * Create SourceLocation from current position (use macros)
     */
    static SourceLocation here(const std::string& file, int line, 
                               const std::string& func) {
        return {file, line, func, {}, {}, -1, 0, {}};
    }
};

// Convenience macros for capturing source locations
#define DEBUG_HERE() ::prosper_debug::SourceLocation::here(__FILE__, __LINE__, __func__)
#define DEBUG_HLE(func_name, func_id) ::prosper_debug::SourceLocation{__FILE__, __LINE__, __func__, {}, (func_name), (func_id), 0, {}}
#define DEBUG_GUEST(addr, sym) ::prosper_debug::SourceLocation{__FILE__, __LINE__, __func__, {}, {}, -1, (addr), (sym)}

// ============================================================================
// Evidence Types - What Do We Know?
// ============================================================================

enum class EvidenceType : uint8_t {
    // Memory events
    MemoryWrite,
    MemoryRead,
    MemoryAllocation,
    MemoryFree,
    MemoryCorruptionDetected,
    
    // State changes
    StateChange,
    ResourceCreated,
    ResourceDestroyed,
    ResourceModified,
    
    // Execution flow
    FunctionEntry,
    FunctionExit,
    BranchTaken,
    ExceptionThrown,
    ExceptionCaught,
    
    // HLE specific
    HLE_CallStart,
    HLE_CallEnd,
    HLE_ContractViolation,
    HLE_SideEffectRecorded,
    HLE_MissingSideEffect,
    
    // System events
    Crash,
    AssertionFailure,
    LogMessage,
    WarningIssued,
    
    // Diagnostic
    DiagnosticRegistered,
    DiagnosticExecuted,
    DiagnosticNotReached,
    DiagnosticFailed,
    
    // User observations
    UserNote,
    HypothesisFormed,
    HypothesisConfirmed,
    HypothesisRejected,
    
    // External
    EnvironmentSnapshot,
    BuildInfo,
    GitCommit,
    ConfigurationChange
};

inline std::string evidenceTypeToString(EvidenceType type) {
    switch (type) {
        case EvidenceType::MemoryWrite: return "MemoryWrite";
        case EvidenceType::MemoryRead: return "MemoryRead";
        case EvidenceType::MemoryAllocation: return "MemoryAllocation";
        case EvidenceType::MemoryFree: return "MemoryFree";
        case EvidenceType::MemoryCorruptionDetected: return "MemoryCorruptionDetected";
        case EvidenceType::StateChange: return "StateChange";
        case EvidenceType::ResourceCreated: return "ResourceCreated";
        case EvidenceType::ResourceDestroyed: return "ResourceDestroyed";
        case EvidenceType::ResourceModified: return "ResourceModified";
        case EvidenceType::FunctionEntry: return "FunctionEntry";
        case EvidenceType::FunctionExit: return "FunctionExit";
        case EvidenceType::BranchTaken: return "BranchTaken";
        case EvidenceType::ExceptionThrown: return "ExceptionThrown";
        case EvidenceType::ExceptionCaught: return "ExceptionCaught";
        case EvidenceType::HLE_CallStart: return "HLE.CallStart";
        case EvidenceType::HLE_CallEnd: return "HLE.CallEnd";
        case EvidenceType::HLE_ContractViolation: return "HLE.ContractViolation";
        case EvidenceType::HLE_SideEffectRecorded: return "HLE.SideEffectRecorded";
        case EvidenceType::HLE_MissingSideEffect: return "HLE.MissingSideEffect";
        case EvidenceType::Crash: return "Crash";
        case EvidenceType::AssertionFailure: return "AssertionFailure";
        case EvidenceType::LogMessage: return "LogMessage";
        case EvidenceType::WarningIssued: return "WarningIssued";
        case EvidenceType::DiagnosticRegistered: return "Diagnostic.Registered";
        case EvidenceType::DiagnosticExecuted: return "Diagnostic.Executed";
        case EvidenceType::DiagnosticNotReached: return "Diagnostic.NotReached";
        case EvidenceType::DiagnosticFailed: return "Diagnostic.Failed";
        case EvidenceType::UserNote: return "UserNote";
        case EvidenceType::HypothesisFormed: return "Hypothesis.Formed";
        case EvidenceType::HypothesisConfirmed: return "Hypothesis.Confirmed";
        case EvidenceType::HypothesisRejected: return "Hypothesis.Rejected";
        case EvidenceType::EnvironmentSnapshot: return "EnvironmentSnapshot";
        case EvidenceType::BuildInfo: return "BuildInfo";
        case EvidenceType::GitCommit: return "GitCommit";
        case EvidenceType::ConfigurationChange: return "ConfigurationChange";
    }
    return "Unknown";
}

// ============================================================================
// Core Evidence Structure
// ============================================================================

/**
 * Single piece of evidence with full provenance
 * 
 * Every evidence item MUST answer:
 * - WHEN did it happen? (timestamp)
 * - WHO caused it? (source_location + subsystem)
 * - WHAT happened? (type + data)
 * - Why is this notable? (severity)
 */
struct Evidence {
    // Unique identifier
    std::string id;
    
    // When
    DebugTimestamp timestamp;
    
    // Who
    Subsystem subsystem{Subsystem::Unknown};
    SourceLocation source;
    std::string thread_id;
    
    // What
    EvidenceType type{EvidenceType::LogMessage};
    std::string description;
    
    // Data payload (JSON-serializable string)
    std::string data_json;
    
    // Significance
    Severity severity{Severity::Info};
    
    // Verification
    bool verified{false};
    std::string verification_method;
    
    // Relationships
    std::string parent_id;       // Causal parent
    std::vector<std::string> related_ids;
    std::vector<std::string> refutes_ids;  // This evidence refutes these hypotheses
    
    // Metadata
    std::map<std::string, std::string> tags;
    std::map<std::string, std::string> metadata;
    
    // Generation
    static std::string generateId() {
        static uint64_t counter = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "evd_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    Evidence() {
        id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    /**
     * Convert to map for JSON serialization
     */
    std::map<std::string, std::string> toMap() const {
        return {
            {"id", id},
            {"timestamp", timestamp.toISO8601()},
            {"frame", std::to_string(timestamp.frame_number)},
            {"subsystem", subsystemToString(subsystem)},
            {"source", source.toString()},
            {"thread_id", thread_id},
            {"type", evidenceTypeToString(type)},
            {"description", description},
            {"data", data_json},
            {"severity", severityToString(severity)},
            {"verified", verified ? "true" : "false"},
            {"parent_id", parent_id}
        };
    }
};

// ============================================================================
// Evidence Collection
// ============================================================================

class EvidenceCollection {
public:
    void add(const Evidence& evidence) {
        m_evidences[evidence.id] = evidence;
        
        // Update indexes
        m_by_type[static_cast<size_t>(evidence.type)].push_back(evidence.id);
        m_by_subsystem[static_cast<size_t>(evidence.subsystem)].push_back(evidence.id);
        
        if (evidence.severity >= Severity::Error) {
            m_errors.push_back(evidence.id);
        }
    }
    
    bool remove(const std::string& id) {
        auto it = m_evidences.find(id);
        if (it == m_evidences.end()) return false;
        
        // Update indexes (slow but correct)
        Evidence evd = it->second;
        auto& type_vec = m_by_type[static_cast<size_t>(evd.type)];
        type_vec.erase(std::remove(type_vec.begin(), type_vec.end(), id), type_vec.end());
        
        auto& sub_vec = m_by_subsystem[static_cast<size_t>(evd.subsystem)];
        sub_vec.erase(std::remove(sub_vec.begin(), sub_vec.end(), id), sub_vec.end());
        
        m_errors.erase(std::remove(m_errors.begin(), m_errors.end(), id), m_errors.end());
        
        m_evidences.erase(it);
        return true;
    }
    
    std::optional<Evidence> find(const std::string& id) const {
        auto it = m_evidences.find(id);
        if (it != m_evidences.end()) return it->second;
        return std::nullopt;
    }
    
    std::vector<Evidence> findByType(EvidenceType type) const {
        std::vector<Evidence> result;
        for (const auto& id : m_by_type[static_cast<size_t>(type)]) {
            if (auto evd = find(id)) result.push_back(*evd);
        }
        return result;
    }
    
    std::vector<Evidence> findBySubsystem(Subsystem subs) const {
        std::vector<Evidence> result;
        for (const auto& id : m_by_subsystem[static_cast<size_t>(subs)]) {
            if (auto evd = find(id)) result.push_back(*evd);
        }
        return result;
    }
    
    std::vector<Evidence> findErrors() const {
        std::vector<Evidence> result;
        for (const auto& id : m_errors) {
            if (auto evd = find(id)) result.push_back(*evd);
        }
        return result;
    }
    
    std::vector<Evidence> findByTimeRange(DebugTimestamp start, DebugTimestamp end) const {
        std::vector<Evidence> result;
        for (const auto& [id, evd] : m_evidences) {
            if (evd.timestamp >= start && evd.timestamp <= end) {
                result.push_back(evd);
            }
        }
        return result;
    }
    
    std::vector<Evidence> all() const {
        std::vector<Evidence> result;
        result.reserve(m_evidences.size());
        for (const auto& [id, evd] : m_evidences) {
            result.push_back(evd);
        }
        return result;
    }
    
    size_t size() const { return m_evidences.size(); }
    bool empty() const { return m_evidences.empty(); }
    
    void clear() {
        m_evidences.clear();
        for (auto& vec : m_by_type) vec.clear();
        for (auto& vec : m_by_subsystem) vec.clear();
        m_errors.clear();
    }

private:
    std::map<std::string, Evidence> m_evidences;
    
    // Indexes for fast queries
    std::array<std::vector<std::string>, 48> m_by_type;      // EvidenceType count
    std::array<std::vector<std::string>, 30> m_by_subsystem;   // Subsystem count
    std::vector<std::string> m_errors;
};

// ============================================================================
// Investigation Status Tracking
// ============================================================================

enum class InvestigationStatus : uint8_t {
    Open,
    InProgress,
    AwaitingEvidence,
    Blocked,
    Confirmed,
    Rejected,
    Superseded,
    Closed
};

inline std::string investigationStatusToString(InvestigationStatus status) {
    switch (status) {
        case InvestigationStatus::Open: return "Open";
        case InvestigationStatus::InProgress: return "InProgress";
        case InvestigationStatus::AwaitingEvidence: return "AwaitingEvidence";
        case InvestigationStatus::Blocked: return "Blocked";
        case InvestigationStatus::Confirmed: return "Confirmed";
        case InvestigationStatus::Rejected: return "Rejected";
        case InvestigationStatus::Superseded: return "Superseded";
        case InvestigationStatus::Closed: return "Closed";
    }
    return "Open";
}

// ============================================================================
// Hypothesis Structure
// ============================================================================

struct Hypothesis {
    std::string id;
    std::string title;
    std::string description;
    InvestigationStatus status{InvestigationStatus::Open};
    
    // Timing
    DebugTimestamp created_at;
    DebugTimestamp updated_at;
    DebugTimestamp confirmed_at;
    DebugTimestamp rejected_at;
    
    // Evidence relationships
    std::vector<std::string> supporting_evidence_ids;
    std::vector<std::string> refuting_evidence_ids;
    
    // Confidence scoring
    double confidence_score{0.0};  // 0.0 to 1.0
    std::string confidence_reasoning;
    
    // Relationships
    std::vector<std::string> related_hypothesis_ids;
    std::string superseded_by_id;
    
    // Notes and findings
    std::string notes;
    std::vector<std::string> rejection_reasons;
    
    // Root cause chain (if confirmed)
    std::vector<std::string> causal_chain_evidence_ids;
    
    static std::string generateId() {
        static uint64_t counter = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "hyp_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    Hypothesis() {
        id = generateId();
        created_at = DebugTimestamp::now();
        updated_at = created_at;
    }
    
    void addSupportingEvidence(const std::string& evidence_id) {
        if (std::find(supporting_evidence_ids.begin(), supporting_evidence_ids.end(), evidence_id) 
            == supporting_evidence_ids.end()) {
            supporting_evidence_ids.push_back(evidence_id);
            updated_at = DebugTimestamp::now();
            updateConfidence();
        }
    }
    
    void addRefutingEvidence(const std::string& evidence_id) {
        if (std::find(refuting_evidence_ids.begin(), refuting_evidence_ids.end(), evidence_id) 
            == refuting_evidence_ids.end()) {
            refuting_evidence_ids.push_back(evidence_id);
            updated_at = DebugTimestamp::now();
            updateConfidence();
        }
    }
    
    void confirm() {
        status = InvestigationStatus::Confirmed;
        confirmed_at = DebugTimestamp::now();
        updated_at = confirmed_at;
        confidence_score = 1.0;
        confidence_reasoning = "Confirmed by direct evidence";
    }
    
    void reject(const std::string& reason = "") {
        status = InvestigationStatus::Rejected;
        rejected_at = DebugTimestamp::now();
        updated_at = rejected_at;
        confidence_score = 0.0;
        if (!reason.empty()) {
            rejection_reasons.push_back(reason);
        }
    }
    
private:
    void updateConfidence() {
        // Simple Bayesian-like update
        double support = supporting_evidence_ids.size();
        double refute = refuting_evidence_ids.size();
        double total = support + refute;
        
        if (total > 0) {
            confidence_score = support / total;
        }
        
        confidence_reasoning = std::to_string(supporting_evidence_ids.size()) + 
                              " supporting, " + 
                              std::to_string(refuting_evidence_ids.size()) + 
                              " refuting";
    }
};

// ============================================================================
// JSON Utilities (Lightweight - No External Dependencies)
// ============================================================================

class JsonUtils {
public:
    static std::string escapeJson(const std::string& input) {
        std::string output;
        output.reserve(input.length() * 2);
        for (char c : input) {
            switch (c) {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                        output += buf;
                    } else {
                        output += c;
                    }
            }
        }
        return output;
    }
    
    static std::string unescapeJson(const std::string& input) {
        std::string output;
        output.reserve(input.length());
        
        for (size_t i = 0; i < input.length(); ++i) {
            if (input[i] == '\\' && i + 1 < input.length()) {
                switch (input[i + 1]) {
                    case '"': output += '"'; i++; break;
                    case '\\': output += '\\'; i++; break;
                    case 'b': output += '\b'; i++; break;
                    case 'f': output += '\f'; i++; break;
                    case 'n': output += '\n'; i++; break;
                    case 'r': output += '\r'; i++; break;
                    case 't': output += '\t'; i++; break;
                    case 'u':
                        if (i + 5 < input.length()) {
                            std::string hex = input.substr(i + 2, 4);
                            unsigned int codepoint;
                            std::istringstream(hex) >> std::hex >> codepoint;
                            output += static_cast<char>(codepoint);
                            i += 5;
                        }
                        break;
                    default:
                        output += input[i];
                }
            } else {
                output += input[i];
            }
        }
        
        return output;
    }
};

} // namespace prosper_debug
