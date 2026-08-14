/**
 * HLE Contract Auditor - Priority 2
 * 
 * PROBLEM SOLVED:
 * Many hours lost because HLE functions returned success without
 * performing required side effects.
 * 
 * EXAMPLE:
 *   sceKernelFtruncate() returns SCE_OK
 *   But: file size unchanged
 *   Result: Guest continues assuming operation succeeded
 * 
 * DESIGN:
 * - Observer only: Does not modify HLE behavior
 * - Validates contracts after HLE calls complete
 * - Reports violations without blocking execution
 * - Tracks missing side effects for root cause analysis
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <deque>

namespace prosper_debug {
namespace contracts {

// ============================================================================
// Contract Types
// ============================================================================

enum class SideEffectType : uint8_t {
    // File system effects
    FileCreated,
    FileDeleted,
    FileSizeChanged,
    FileHandleValid,
    FilePositionChanged,
    
    // Memory effects
    MemoryAllocated,
    MemoryFreed,
    MemoryRegionModified,
    HandleCreated,
    HandleDestroyed,
    HandleValid,
    
    // Synchronization effects
    MutexAcquired,
    MutexReleased,
    SemaphoreSignaled,
    SemaphoreWaited,
    ConditionVariableTriggered,
    
    // Thread effects
    ThreadCreated,
    ThreadStarted,
    ThreadTerminated,
    ThreadPriorityChanged,
    
    // GPU effects
    GpuResourceCreated,
    GpuCommandSubmitted,
    GpuMemoryMapped,
    GpuMemoryUnmapped,
    GpuFenceSignaled,
    
    // State effects
    StateVariableSet,
    CallbackRegistered,
    EventQueued,
    
    // Generic (for custom validation)
    CustomEffect,
    EffectVerified
};

inline std::string sideEffectTypeToString(SideEffectType type) {
    switch (type) {
        case SideEffectType::FileCreated: return "FileCreated";
        case SideEffectType::FileDeleted: return "FileDeleted";
        case SideEffectType::FileSizeChanged: return "FileSizeChanged";
        case SideEffectType::FileHandleValid: return "FileHandleValid";
        case SideEffectType::FilePositionChanged: return "FilePositionChanged";
        case SideEffectType::MemoryAllocated: return "MemoryAllocated";
        case SideEffectType::MemoryFreed: return "MemoryFreed";
        case SideEffectType::MemoryRegionModified: return "MemoryRegionModified";
        case SideEffectType::HandleCreated: return "HandleCreated";
        case SideEffectType::HandleDestroyed: return "HandleDestroyed";
        case SideEffectType::HandleValid: return "HandleValid";
        case SideEffectType::MutexAcquired: return "MutexAcquired";
        case SideEffectType::MutexReleased: return "MutexReleased";
        case SideEffectType::SemaphoreSignaled: return "SemaphoreSignaled";
        case SideEffectType::SemaphoreWaited: return "SemaphoreWaited";
        case SideEffectType::ConditionVariableTriggered: return "ConditionVariableTriggered";
        case SideEffectType::ThreadCreated: return "ThreadCreated";
        case SideEffectType::ThreadStarted: return "ThreadStarted";
        case SideEffectType::ThreadTerminated: return "ThreadTerminated";
        case SideEffectType::ThreadPriorityChanged: return "ThreadPriorityChanged";
        case SideEffectType::GpuResourceCreated: return "GpuResourceCreated";
        case SideEffectType::GpuCommandSubmitted: return "GpuCommandSubmitted";
        case SideEffectType::GpuMemoryMapped: return "GpuMemoryMapped";
        case SideEffectType::GpuMemoryUnmapped: return "GpuMemoryUnmapped";
        case SideEffectType::GpuFenceSignaled: return "GpuFenceSignaled";
        case SideEffectType::StateVariableSet: return "StateVariableSet";
        case SideEffectType::CallbackRegistered: return "CallbackRegistered";
        case SideEffectType::EventQueued: return "EventQueued";
        case SideEffectType::CustomEffect: return "CustomEffect";
        case SideEffectType::EffectVerified: return "EffectVerified";
    }
    return "Unknown";
}

// ============================================================================
// Contract Definition
// ============================================================================

/**
 * Defines expected contract for an HLE function
 */
struct HLEContract {
    std::string function_name;
    int function_id{-1};
    std::string library_name;
    std::string description;
    
    // Expected side effects (must have at least one)
    struct ExpectedEffect {
        SideEffectType type;
        std::string description;
        bool required{true};           // Must happen vs optional
        std::map<std::string, std::string> parameters;  // Type-specific params
        
        // Validation function (returns true if effect satisfied)
        std::function<bool()> validator;
        
        // For delayed validation (e.g., async operations)
        bool validate_immediately{true};
        uint64_t timeout_ms{0};        // 0 = no timeout
    };
    
    std::vector<ExpectedEffect> expected_effects;
    
    // Return value constraints
    struct ReturnValueConstraint {
        bool check_return_value{false};
        std::set<int> success_codes;     // These codes indicate success
        std::set<int> failure_codes;     // These codes indicate known failures
        bool allow_other_codes{false};   // Unknown codes are warnings or errors
    };
    
    ReturnValueConstraint return_constraint;
    
    // Pre-conditions (should be true before call)
    std::vector<std::function<bool()>> preconditions;
    
    // Post-conditions (should be true after call)
    std::vector<std::function<bool()>> postconditions;
};

// ============================================================================
// Side Effect Record
// ============================================================================

struct SideEffectRecord {
    std::string id;
    DebugTimestamp timestamp;
    SideEffectType type;
    
    // What happened
    std::string description;
    std::map<std::string, std::string> data;  // Type-specific data
    
    // Provenance
    Subsystem subsystem{Subsystem::HLE_Other};
    SourceLocation source;
    std::string hle_function_name;
    
    // Verification
    bool verified{false};
    std::string verification_detail;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "se_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    SideEffectRecord() {
        id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"timestamp\": \"" << timestamp.toISO8601() << "\",\n";
        json << "  \"type\": \"" << sideEffectTypeToString(type) << "\",\n";
        json << "  \"description\": \"" << JsonUtils::escapeJson(description) << "\",\n";
        json << "  \"verified\": " << (verified ? "true" : "false") << ",\n";
        json << "  \"hle_function\": \"" << JsonUtils::escapeJson(hle_function_name) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// HLE Call Record
// ============================================================================

enum class CallStatus : uint8_t {
    Started,
    Completed,
    ContractPassed,
    ContractFailed,
    MissingSideEffects,
    ExceptionDuringCall,
    Timeout
};

inline std::string callStatusToString(CallStatus status) {
    switch (status) {
        case CallStatus::Started: return "Started";
        case CallStatus::Completed: return "Completed";
        case CallStatus::ContractPassed: return "ContractPassed";
        case CallStatus::ContractFailed: return "ContractFailed";
        case CallStatus::MissingSideEffects: return "MissingSideEffects";
        case CallStatus::ExceptionDuringCall: return "ExceptionDuringCall";
        case CallStatus::Timeout: return "Timeout";
    }
    return "Unknown";
}

struct HLECallRecord {
    std::string id;
    DebugTimestamp start_time;
    DebugTimestamp end_time;
    
    // Function identification
    std::string function_name;
    int function_id{-1};
    std::string library_name;
    
    // Call parameters (serialized)
    std::string parameters_json;
    
    // Result
    int return_value{0};
    CallStatus status{CallStatus::Started};
    std::string error_message;
    
    // Contract validation
    std::vector<SideEffectRecord> observed_effects;
    std::vector<std::string> missing_required_effects;
    std::vector<std::string> failed_validations;
    
    // Provenance
    SourceLocation caller_location;
    std::string thread_id;
    uint64_t guest_call_site{0};
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "call_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    HLECallRecord() {
        id = generateId();
        start_time = DebugTimestamp::now();
    }
    
    /**
     * Calculate duration in microseconds
     */
    uint64_t durationMicroseconds() const {
        if (end_time.steady_clock.time_since_epoch().count() == 0) {
            return 0;  // Not yet completed
        }
        auto duration = end_time.steady_clock - start_time.steady_clock;
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"function\": \"" << JsonUtils::escapeJson(function_name) << "\",\n";
        json << "  \"library\": \"" << JsonUtils::escapeJson(library_name) << "\",\n";
        json << "  \"return_value\": " << return_value << ",\n";
        json << "  \"status\": \"" << callStatusToString(status) << "\",\n";
        json << "  \"duration_us\": " << durationMicroseconds() << ",\n";
        json << "  \"observed_effects\": " << observed_effects.size() << ",\n";
        json << "  \"missing_effects\": " << missing_required_effects.size() << "\n";
        
        if (!missing_required_effects.empty()) {
            json << ",\n  \"missing_effect_details\": [\n";
            for (size_t i = 0; i < missing_required_effects.size(); ++i) {
                json << "    \"" << JsonUtils::escapeJson(missing_required_effects[i]) << "\"";
                if (i < missing_required_effects.size() - 1) json << ",";
                json << "\n";
            }
            json << "  ]";
        }
        
        json << "\n}";
        return json.str();
    }
};

// ============================================================================
// Violation Report
// ============================================================================

struct ContractViolation {
    HLECallRecord call_record;
    HLEContract contract;
    
    // What went wrong
    std::vector<std::string> missing_side_effects;
    std::vector<std::string> failed_postconditions;
    std::vector<std::string> unexpected_errors;
    std::vector<std::string> failed_validations;
    
    // Severity assessment
    Severity severity{Severity::Warning};
    std::string impact_description;
    
    // Suggested fix (if detectable)
    std::string suggested_fix;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"function\": \"" << JsonUtils::escapeJson(call_record.function_name) << "\",\n";
        json << "  \"severity\": \"" << severityToString(severity) << "\",\n";
        json << "  \"missing_side_effects\": " << missing_side_effects.size() << ",\n";
        json << "  \"impact\": \"" << JsonUtils::escapeJson(impact_description) << "\",\n";
        json << "  \"suggested_fix\": \"" << JsonUtils::escapeJson(suggested_fix) << "\"\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// HLE Contract Auditor - Main Class
// ============================================================================

class HLEContractAuditor {
public:
    struct Config {
        bool enabled;
        bool strict_mode;          // Fail on any missing effect
        bool log_all_calls;        // Log even passing calls
        bool auto_register_common;  // Auto-register contracts for common HLE
        size_t max_history;        // Max calls to keep
        
        Config()
            : enabled(true)
            , strict_mode(false)
            , log_all_calls(false)
            , auto_register_common(true)
            , max_history(10000) {}
        
        Config(bool en, bool sm, bool lac, bool arc, size_t mh)
            : enabled(en)
            , strict_mode(sm)
            , log_all_calls(lac)
            , auto_register_common(arc)
            , max_history(mh) {}
        
        static Config lenient() {
            return Config(true, false, false, true, 50000);
        }
        
        static Config strict() {
            return Config(true, true, true, true, 100000);
        }
    };
    
    explicit HLEContractAuditor(const Config& config = Config())
        : m_config(config), m_enabled(config.enabled) {}
    
    // ========================================================================
    // Control Interface
    // ========================================================================
    
    void enable() { m_enabled = true; }
    void disable() { m_enabled = false; }
    bool isEnabled() const { return m_enabled; }
    
    void clear() {
        std::unique_lock lock(m_mutex);
        m_call_history.clear();
        m_active_calls.clear();
        m_violations.clear();
        m_contracts.clear();
    }
    
    // ========================================================================
    // Contract Registration
    // ========================================================================
    
    /**
     * Register a contract for an HLE function
     */
    void registerContract(const HLEContract& contract) {
        std::unique_lock lock(m_mutex);
        
        std::string key = makeContractKey(contract.function_name, contract.library_name);
        m_contracts[key] = contract;
    }
    
    /**
     * Register a simple contract with just required side effects
     */
    void registerSimpleContract(
        const std::string& function_name,
        const std::string& library_name,
        const std::vector<SideEffectType>& required_effects,
        const std::set<int>& success_codes = {0}) {
        
        HLEContract contract;
        contract.function_name = function_name;
        contract.library_name = library_name;
        contract.return_constraint.check_return_value = true;
        contract.return_constraint.success_codes = success_codes;
        
        for (auto effect_type : required_effects) {
            HLEContract::ExpectedEffect effect;
            effect.type = effect_type;
            effect.required = true;
            effect.description = sideEffectTypeToString(effect_type);
            contract.expected_effects.push_back(effect);
        }
        
        registerContract(contract);
    }
    
    /**
     * Pre-register common HLE contracts
     */
    void registerCommonContracts() {
        // File operations
        registerSimpleContract("sceKernelOpen", "libkernel", 
            {SideEffectType::FileHandleValid}, {0});
        registerSimpleContract("sceKernelClose", "libkernel",
            {}, {0});  // Close may not have observable effects
        registerSimpleContract("sceKernelRead", "libkernel",
            {SideEffectType::FilePositionChanged});
        registerSimpleContract("sceKernelWrite", "libkernel",
            {SideEffectType::FilePositionChanged, SideEffectType::FileSizeChanged});
        registerSimpleContract("sceKernelFtruncate", "libkernel",
            {SideEffectType::FileSizeChanged});
        
        // Memory operations
        registerSimpleContract("sceKernelAllocateDirectMemory", "libkernel",
            {SideEffectType::MemoryAllocated});
        registerSimpleContract("sceKernelFreeDirectMemory", "libkernel",
            {SideEffectType::MemoryFreed});
        registerSimpleContract("sceKernelMapDirectMemory", "libkernel",
            {SideEffectType::MemoryRegionModified});
        
        // Synchronization
        registerSimpleContract("sceKernelCreateMutex", "libkernel",
            {SideEffectType::HandleCreated});
        registerSimpleContract("sceKernelCreateSema", "libkernel",
            {SideEffectType::HandleCreated});
        registerSimpleContract("sceKernelCreateCond", "libkernel",
            {SideEffectType::HandleCreated});
        
        // Thread operations
        registerSimpleContract("sceKernelCreateThread", "libkernel",
            {SideEffectType::ThreadCreated, SideEffectType::HandleCreated});
        
        // GPU operations
        registerSimpleContract("sceGnmSubmitCommandBuffers", "libgnm",
            {SideEffectType::GpuCommandSubmitted});
        registerSimpleContract("sceGnmAddReleaseQueue", "libgnm",
            {});
    }
    
    // ========================================================================
    // Call Tracking Interface (Call From HLE Wrappers)
    // ========================================================================
    
    /**
     * Start tracking an HLE call
     * Returns call ID to use when completing the call
     */
    std::string startCall(
        const std::string& function_name,
        int function_id,
        const std::string& library_name,
        const SourceLocation& caller = DEBUG_HERE(),
        const std::string& params_json = "{}") {
        
        if (!isEnabled()) return "";
        
        HLECallRecord record;
        record.function_name = function_name;
        record.function_id = function_id;
        record.library_name = library_name;
        record.caller_location = caller;
        record.parameters_json = params_json;
        record.thread_id = getCurrentThreadId();
        record.status = CallStatus::Started;
        
        std::unique_lock lock(m_mutex);
        m_active_calls[record.id] = record;
        
        return record.id;
    }
    
    /**
     * Complete an HLE call and validate contract
     */
    void completeCall(
        const std::string& call_id,
        int return_value,
        const std::vector<SideEffectRecord>& observed_effects = {}) {
        
        if (!isEnabled() || call_id.empty()) return;
        
        std::unique_lock lock(m_mutex);
        
        auto it = m_active_calls.find(call_id);
        if (it == m_active_calls.end()) return;
        
        HLECallRecord& record = it->second;
        record.return_value = return_value;
        record.end_time = DebugTimestamp::now();
        record.observed_effects = observed_effects;
        
        // Validate against registered contract
        validateCall(record);
        
        // Move from active to history
        m_call_history.push_back(record);
        m_active_calls.erase(it);
        
        // Enforce history limit
        while (m_call_history.size() > m_config.max_history) {
            m_call_history.pop_front();
        }
    }
    
    /**
     * Convenience: Complete call with no side effects recorded
     */
    void completeCallNoEffects(const std::string& call_id, int return_value) {
        completeCall(call_id, return_value, {});
    }
    
    /**
     * Record that a specific side effect occurred during a call
     * (Can be called between startCall and completeCall)
     */
    void recordSideEffect(
        const std::string& call_id,
        SideEffectType type,
        const std::string& description = "",
        const std::map<std::string, std::string>& data = {},
        const SourceLocation& source = DEBUG_HERE()) {
        
        if (!isEnabled() || call_id.empty()) return;
        
        std::unique_lock lock(m_mutex);
        
        auto it = m_active_calls.find(call_id);
        if (it == m_active_calls.end()) return;
        
        SideEffectRecord effect;
        effect.type = type;
        effect.description = description.empty() ? sideEffectTypeToString(type) : description;
        effect.data = data;
        effect.source = source;
        effect.hle_function_name = it->second.function_name;
        effect.verified = true;
        
        it->second.observed_effects.push_back(effect);
    }
    
    // ========================================================================
    // Query Interface
    // ========================================================================
    
    /**
     * Get all contract violations
     */
    std::vector<ContractViolation> getViolations() const {
        std::shared_lock lock(m_mutex);
        return m_violations;
    }
    
    /**
     * Get violations for a specific function
     */
    std::vector<ContractViolation> getViolationsForFunction(
        const std::string& function_name) const {
        
        std::shared_lock lock(m_mutex);
        std::vector<ContractViolation> result;
        
        for (const auto& violation : m_violations) {
            if (violation.call_record.function_name == function_name) {
                result.push_back(violation);
            }
        }
        
        return result;
    }
    
    /**
     * Get call history for a function
     */
    std::vector<HLECallRecord> getCallHistory(
        const std::string& function_name,
        size_t limit = 100) const {
        
        std::shared_lock lock(m_mutex);
        std::vector<HLECallRecord> result;
        
        // Search from most recent
        for (auto it = m_call_history.rbegin(); 
             it != m_call_history.rend() && result.size() < limit; ++it) {
            
            if (it->function_name == function_name) {
                result.push_back(*it);
            }
        }
        
        return result;
    }
    
    /**
     * Get statistics about contract compliance
     */
    struct ComplianceStats {
        size_t total_calls{0};
        size_t passed_calls{0};
        size_t failed_calls{0};
        size_t calls_with_missing_effects{0};
        
        // By library
        std::map<std::string, size_t> calls_by_library;
        std::map<std::string, size_t> violations_by_library;
        
        // Most problematic functions
        std::vector<std::pair<std::string, size_t>> top_violations;
        
        double pass_rate() const {
            return total_calls > 0 ? 
                static_cast<double>(passed_calls) / total_calls : 1.0;
        }
        
        std::string toJson() const {
            std::stringstream json;
            json << "{\n";
            json << "  \"total_calls\": " << total_calls << ",\n";
            json << "  \"passed\": " << passed_calls << ",\n";
            json << "  \"failed\": " << failed_calls << ",\n";
            json << "  \"pass_rate\": " << std::fixed << std::setprecision(2) 
                 << (pass_rate() * 100.0) << "%\n";
            json << "}";
            return json.str();
        }
    };
    
    ComplianceStats getComplianceStats() const {
        std::shared_lock lock(m_mutex);
        
        ComplianceStats stats;
        stats.total_calls = m_call_history.size();
        
        // Count by function for top violators
        std::map<std::string, size_t> violation_counts;
        
        for (const auto& call : m_call_history) {
            stats.calls_by_library[call.library_name]++;
            
            switch (call.status) {
                case CallStatus::ContractPassed:
                    stats.passed_calls++;
                    break;
                case CallStatus::ContractFailed:
                case CallStatus::MissingSideEffects:
                    stats.failed_calls++;
                    stats.calls_with_missing_effects++;
                    violation_counts[call.function_name]++;
                    stats.violations_by_library[call.library_name]++;
                    break;
                default:
                    break;
            }
        }
        
        // Sort for top violators
        using PairType = std::pair<std::string, size_t>;
        std::vector<PairType> sorted(violation_counts.begin(), violation_counts.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const PairType& a, const PairType& b) { return b.second < a.second; });
        
        stats.top_violations.assign(sorted.begin(), 
            sorted.begin() + std::min(sorted.size(), size_t(10)));
        
        return stats;
    }
    
    // ========================================================================
    // Export
    // ========================================================================
    
    /**
     * Export all data as JSON
     */
    std::string exportToJson() const {
        std::shared_lock lock(m_mutex);
        
        std::stringstream json;
        json << "{\n";
        json << "  \"export_timestamp\": \"" << DebugTimestamp::now().toISO8601() << "\",\n";
        
        // Statistics
        auto stats = getComplianceStats();
        json << "  \"statistics\": " << stats.toJson() << ",\n";
        
        // Recent violations
        json << "  \"recent_violations\": [\n";
        size_t count = 0;
        for (const auto& violation : m_violations) {
            if (count++ > 50) break;  // Limit output
            json << "    " << violation.toJson();
            if (count < m_violations.size() && count <= 50) json << ",";
            json << "\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        return json.str();
    }

private:
    Config m_config;
    std::atomic<bool> m_enabled;
    mutable std::shared_mutex m_mutex;
    
    // Registered contracts
    std::map<std::string, HLEContract> m_contracts;
    
    // Active calls (in progress)
    std::map<std::string, HLECallRecord> m_active_calls;
    
    // Completed call history
    std::deque<HLECallRecord> m_call_history;
    
    // Contract violations
    std::vector<ContractViolation> m_violations;
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    std::string makeContractKey(const std::string& func, const std::string& lib) {
        return lib + "::" + func;
    }
    
    void validateCall(HLECallRecord& record) {
        std::string key = makeContractKey(record.function_name, record.library_name);
        
        auto contract_it = m_contracts.find(key);
        if (contract_it == m_contracts.end()) {
            // No contract registered - mark as passed (or warning in strict mode)
            record.status = m_config.strict_mode ? 
                CallStatus::MissingSideEffects : CallStatus::Completed;
            return;
        }
        
        const HLEContract& contract = contract_it->second;
        ContractViolation violation;
        violation.call_record = record;
        violation.contract = contract;
        
        bool has_failure = false;
        
        // Check each expected effect
        for (const auto& expected : contract.expected_effects) {
            if (!expected.required) continue;
            
            bool found = false;
            for (const auto& observed : record.observed_effects) {
                if (observed.type == expected.type) {
                    found = true;
                    
                    // Run custom validator if provided
                    if (expected.validator && !expected.validator()) {
                        violation.failed_validations.push_back(
                            "Validator failed for: " + expected.description);
                        has_failure = true;
                    }
                    
                    break;
                }
            }
            
            if (!found) {
                violation.missing_side_effects.push_back(expected.description);
                has_failure = true;
                
                // Also add to call record
                record.missing_required_effects.push_back(expected.description);
            }
        }
        
        // Check return value constraint
        if (contract.return_constraint.check_return_value) {
            bool is_success = contract.return_constraint.success_codes.count(record.return_value) > 0;
            bool is_known_failure = contract.return_constraint.failure_codes.count(record.return_value) > 0;
            
            if (!is_success && !is_known_failure && !contract.return_constraint.allow_other_codes) {
                violation.unexpected_errors.push_back(
                    "Unexpected return value: " + std::to_string(record.return_value));
                has_failure = true;
            }
        }
        
        // Determine final status
        if (has_failure) {
            record.status = violation.missing_side_effects.empty() ?
                CallStatus::ContractFailed : CallStatus::MissingSideEffects;
            
            // Assess severity
            assessViolationSeverity(violation);
            
            // Store violation
            m_violations.push_back(violation);
            
            // Keep violations bounded
            while (m_violations.size() > m_config.max_history) {
                m_violations.erase(m_violations.begin());
            }
        } else {
            record.status = CallStatus::ContractPassed;
        }
    }
    
    void assessViolationSeverity(ContractViolation& violation) {
        // Critical: File handle claimed valid but wasn't created
        for (const auto& missing : violation.missing_side_effects) {
            if (missing.find("FileHandle") != std::string::npos ||
                missing.find("Handle") != std::string::npos) {
                violation.severity = Severity::Error;
                violation.impact_description = 
                    "Guest may use invalid handle, leading to crashes or corruption";
                violation.suggested_fix =
                    "Ensure handle is properly created and tracked before returning success";
                return;
            }
        }
        
        // Warning: State change not performed
        for (const auto& missing : violation.missing_side_effects) {
            if (missing.find("State") != std::string::npos ||
                missing.find("Modified") != std::string::npos) {
                violation.severity = Severity::Warning;
                violation.impact_description =
                    "Guest may operate on stale state, causing incorrect behavior";
                return;
            }
        }
        
        // Default
        violation.severity = Severity::Info;
        violation.impact_description =
            "HLE function may not fully implement expected behavior";
    }
    
    static std::string getCurrentThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
};

} // namespace contracts
} // namespace prosper_debug
