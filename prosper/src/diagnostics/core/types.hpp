// diagnostics/core/types.hpp — Core type definitions for AI-Oriented Diagnostics Platform
//
// Universal event schema, severity levels, subsystem identifiers, and phase definitions.
// Every diagnostic event uses these types to ensure consistency across all collectors.
#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <vector>
#include <unordered_map>

namespace prosper {
namespace diagnostics {

// --- Severity Levels ----------------------------------------------------------
enum class Severity : uint8_t {
    DEBUG     = 0,  // Verbose tracing information
    INFO      = 1,  // Normal operational events
    WARNING   = 2,  // Potential issues that don't prevent execution
    ERROR     = 3,  // Failures that may affect functionality
    CRITICAL  = 4,  // Serious failures that stop or crash execution
};

constexpr const char* severity_string(Severity s) {
    switch (s) {
        case Severity::DEBUG:    return "DEBUG";
        case Severity::INFO:     return "INFO";
        case Severity::WARNING:  return "WARNING";
        case Severity::ERROR:    return "ERROR";
        case Severity::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

// --- Subsystem Identifiers ---------------------------------------------------
enum class Subsystem : uint8_t {
    UNKNOWN     = 0,
    CORE        = 1,   // Diagnostics system itself
    LOADER      = 2,   // ELF/PRX loader
    LINKER      = 3,   // Dynamic linker
    MEMORY      = 4,   // Memory manager
    THREAD      = 5,   // Thread system
    SYNC        = 6,   // Synchronization primitives
    HLE         = 7,   // High-level emulation
    SYSCALL     = 8,   // System call dispatcher
    GPU         = 9,   // GPU pipeline
    VIDEO       = 10,  // VideoOut/presentation
    AUDIO       = 11,  // Audio system
    FILESYSTEM  = 12,  // File I/O
    NETWORK     = 13,  // Network stack
    INPUT       = 14,  // Input handling
    CRASH       = 15,  // Crash handler
};

constexpr const char* subsystem_string(Subsystem s) {
    switch (s) {
        case Subsystem::UNKNOWN:    return "core";
        case Subsystem::CORE:       return "core";
        case Subsystem::LOADER:     return "loader";
        case Subsystem::LINKER:     return "linker";
        case Subsystem::MEMORY:     return "memory";
        case Subsystem::THREAD:     return "thread";
        case Subsystem::SYNC:       return "sync";
        case Subsystem::HLE:        return "hle";
        case Subsystem::SYSCALL:    return "syscall";
        case Subsystem::GPU:        return "gpu";
        case Subsystem::VIDEO:      return "video";
        case Subsystem::AUDIO:      return "audio";
        case Subsystem::FILESYSTEM: return "filesystem";
        case Subsystem::NETWORK:    return "network";
        case Subsystem::INPUT:      return "input";
        case Subsystem::CRASH:      return "crash";
    }
    return "unknown";
}

// --- Boot Phases (Timeline State Machine) ------------------------------------
enum class BootPhase : uint16_t {
    PROCESS_START          = 0,
    ELF_OPENED             = 1,
    ELF_PARSED             = 2,
    SEGMENTS_MAPPED        = 3,
    RELOCATIONS_APPLIED    = 4,
    PRX_LOADING            = 5,
    IMPORT_RESOLUTION      = 6,
    THREAD_CREATION        = 7,
    ENTRYPOINT_EXECUTED    = 8,
    RUNTIME_INITIALIZED    = 9,
    VIDEOOUT_INITIALIZED   = 10,
    FIRST_FRAME_ATTEMPT    = 11,
    FIRST_FRAME_CAPTURED   = 12,
    BOOT_COMPLETE          = 13,
    
    // Failure states (offset by 100 to distinguish from normal phases)
    PHASE_FAILED           = 255,
};

constexpr const char* boot_phase_string(BootPhase p) {
    switch (p) {
        case BootPhase::PROCESS_START:        return "PROCESS_START";
        case BootPhase::ELF_OPENED:           return "ELF_OPENED";
        case BootPhase::ELF_PARSED:           return "ELF_PARSED";
        case BootPhase::SEGMENTS_MAPPED:      return "SEGMENTS_MAPPED";
        case BootPhase::RELOCATIONS_APPLIED:  return "RELOCATIONS_APPLIED";
        case BootPhase::PRX_LOADING:          return "PRX_LOADING";
        case BootPhase::IMPORT_RESOLUTION:    return "IMPORT_RESOLUTION";
        case BootPhase::THREAD_CREATION:      return "THREAD_CREATION";
        case BootPhase::ENTRYPOINT_EXECUTED:  return "ENTRYPOINT_EXECUTED";
        case BootPhase::RUNTIME_INITIALIZED:  return "RUNTIME_INITIALIZED";
        case BootPhase::VIDEOOUT_INITIALIZED: return "VIDEOOUT_INITIALIZED";
        case BootPhase::FIRST_FRAME_ATTEMPT:  return "FIRST_FRAME_ATTEMPT";
        case BootPhase::FIRST_FRAME_CAPTURED: return "FIRST_FRAME_CAPTURED";
        case BootPhase::BOOT_COMPLETE:        return "BOOT_COMPLETE";
        case BootPhase::PHASE_FAILED:         return "PHASE_FAILED";
    }
    return "UNKNOWN";
}

// --- Event Structure ---------------------------------------------------------
struct EventId {
    uint64_t        id;          // Monotonically increasing event ID
    std::chrono::steady_clock::time_point timestamp;
    
    EventId() : id(0), timestamp(std::chrono::steady_clock::now()) {}
    explicit EventId(uint64_t i) : id(i), timestamp(std::chrono::steady_clock::now()) {}
};

struct SourceLocation {
    std::string file;
    int         line = 0;
    std::string function;
    
    SourceLocation() = default;
    SourceLocation(const char* f, int l, const char* fn = "")
        : file(f ? f : ""), line(l), function(fn ? fn : "") {}
};

// Universal event format - every diagnostic event uses this structure
struct DiagnosticEvent {
    EventId         event_id;
    std::string     type;           // e.g., "PRX_LOAD", "HLE_CALL", "MEMORY_ALLOC"
    Severity        severity = Severity::INFO;
    Subsystem       subsystem = Subsystem::UNKNOWN;
    std::string     thread_name;    // Thread that generated the event
    SourceLocation  source;         // Source code location
    std::string     message;        // Human-readable description
    
    // Structured data payload (JSON-serializable)
    struct DataField {
        std::string key;
        enum Type { STRING, INT, UINT, FLOAT, BOOL, ARRAY } type;
        union {
            int64_t     int_val;
            uint64_t    uint_val;
            double      float_val;
            bool        bool_val;
        };
        std::string string_val;
        std::vector<DataField> array_val;
        
        DataField() : type(STRING), int_val(0) {}
    };
    std::vector<DataField> data;
    
    // Evidence references (IDs of evidence items that support this event)
    std::vector<uint64_t> evidence_refs;
    
    // Confidence score (0.0 - 1.0) for AI analysis
    double confidence = 1.0;
    
    DiagnosticEvent() = default;
    
    // Helper methods for building events
    void add_string(const std::string& key, const std::string& value) {
        DataField f; f.type = DataField::STRING; f.key = key; f.string_val = value;
        data.push_back(f);
    }
    void add_int(const std::string& key, int64_t value) {
        DataField f; f.type = DataField::INT; f.key = key; f.int_val = value;
        data.push_back(f);
    }
    void add_uint(const std::string& key, uint64_t value) {
        DataField f; f.type = DataField::UINT; f.key = key; f.uint_val = value;
        data.push_back(f);
    }
    void add_float(const std::string& key, double value) {
        DataField f; f.type = DataField::FLOAT; f.key = key; f.float_val = value;
        data.push_back(f);
    }
    void add_bool(const std::string& key, bool value) {
        DataField f; f.type = DataField::BOOL; f.key = key; f.bool_val = value;
        data.push_back(f);
    }
};

// --- Timeline Entry ----------------------------------------------------------
struct TimelineEntry {
    BootPhase                    phase;
    BootPhase                    from_phase = BootPhase::PROCESS_START;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    bool                         success = false;
    std::string                  error_message;
    uint64_t                     duration_ms = 0;
    uint64_t                     event_id = 0;  // Primary event ID for this transition
    
    // Phase-specific structured data
    struct PhaseData {
        std::vector<std::pair<std::string, std::string>> fields;
        
        void add(const std::string& k, const std::string& v) {
            fields.emplace_back(k, v);
        }
    } data;
};

// --- Evidence Item -----------------------------------------------------------
enum class EvidenceType : uint8_t {
    SCREENSHOT     = 0,  // Captured frame image
    MEMORY_DUMP    = 1,  // Memory region dump
    REGISTER_STATE = 2,  // CPU register state
    STACK_TRACE    = 3,  // Call stack
    LOG_FRAGMENT   = 4,  // Relevant log lines
    METRIC         = 5,  // Numeric measurement
    CUSTOM         = 99, // User-defined evidence
};

struct EvidenceItem {
    uint64_t              id;
    EvidenceType          type;
    std::string           description;
    std::string           file_path;      // Path to stored evidence (relative to output dir)
    std::string           mime_type;
    std::vector<uint64_t> related_events; // Events this evidence supports
    std::chrono::steady_clock::time_point captured_at;
    
    EvidenceItem() : id(0), type(EvidenceType::CUSTOM),
                      captured_at(std::chrono::steady_clock::now()) {}
};

// --- Session Metadata --------------------------------------------------------
struct DiagnosticSession {
    std::string     session_id;
    std::chrono::system_clock::time_point timestamp;
    std::string     game_identifier;    // e.g., "PPSA02451" or path-based
    std::string     binary_hash;        // Hash of main executable
    std::string     configuration;      // CLI flags and options summary
    std::string     platform;           // Host platform
    std::string     git_revision;       // Build git commit
    std::string     prosper_version;    // Version string
    std::string     output_directory;   // Where diagnostics are written
    
    // Runtime statistics
    struct Stats {
        uint64_t total_events = 0;
        uint64_t total_evidence = 0;
        uint64_t errors = 0;
        uint64_t warnings = 0;
        BootPhase final_phase = BootPhase::PROCESS_START;
        std::chrono::steady_clock::time_point boot_start;
        std::chrono::steady_clock::time_point boot_end_or_now;
    } stats;
    
    DiagnosticSession()
        : timestamp(std::chrono::system_clock::now()),
          stats{} {}
};

} // namespace diagnostics
} // namespace prosper
