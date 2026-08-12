#pragma once

/**
 * @file runtime_init_trace_plugin.hpp
 * @brief Runtime Initialization Trace Plugin - Tracks DT_INIT/constructor execution
 * 
 * Phase 11 Enhancement - Based on real PS4/PS5 debugging experience
 * 
 * Real crashes often happen before main/game initialization during:
 * - DT_INIT execution
 * - DT_INIT_ARRAY iteration  
 * - DT_SCE_INIT_ARRAY (PS4-specific)
 * - C++ constructor execution (__mod_init_func, etc.)
 * - CRT initialization (malloc, atexit, thread-local storage setup)
 * - Destructor registration (atexit, __cxa_atexit)
 */

#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Initialization Stage Enumeration
//=============================================================================

enum class InitStage : uint8_t {
    NOT_STARTED = 0,
    ELF_DYNAMIC_LINKING,       // Dynamic linker setup
    DT_INIT,                   // Single init function (DT_INIT)
    DT_INIT_ARRAY_PREPARE,     // Preparing init array iteration
    DT_INIT_ARRAY_EXECUTING,   // Executing DT_INIT_ARRAY entries
    DT_SCE_INIT_ARRAY,         // PS4-specific init array
    CPP_CONSTRUCTORS,          // C++ global constructors (__mod_init_func)
    TLS_SETUP,                 // Thread-local storage initialization
    CRT_INIT,                  // C runtime initialization (atexit, etc.)
    LIBRARY_INITIALIZERS,      // Library-specific initializers
    PRE_MAIN,                  // Just before main() entry
    MAIN_ENTRY,                // main() function entered
    INIT_COMPLETE,             // All initialization done
    INIT_FAILED,               // Initialization failed/crashed
    UNKNOWN = 255
};

inline const char* init_stage_to_string(InitStage stage) {
    switch (stage) {
        case InitStage::NOT_STARTED: return "NOT_STARTED";
        case InitStage::ELF_DYNAMIC_LINKING: return "ELF_DYNAMIC_LINKING";
        case InitStage::DT_INIT: return "DT_INIT";
        case InitStage::DT_INIT_ARRAY_PREPARE: return "DT_INIT_ARRAY_PREPARE";
        case InitStage::DT_INIT_ARRAY_EXECUTING: return "DT_INIT_ARRAY_EXECUTING";
        case InitStage::DT_SCE_INIT_ARRAY: return "DT_SCE_INIT_ARRAY";
        case InitStage::CPP_CONSTRUCTORS: return "CPP_CONSTRUCTORS";
        case InitStage::TLS_SETUP: return "TLS_SETUP";
        case InitStage::CRT_INIT: return "CRT_INIT";
        case InitStage::LIBRARY_INITIALIZERS: return "LIBRARY_INITIALIZERS";
        case InitStage::PRE_MAIN: return "PRE_MAIN";
        case InitStage::MAIN_ENTRY: return "MAIN_ENTRY";
        case InitStage::INIT_COMPLETE: return "INIT_COMPLETE";
        case InitStage::INIT_FAILED: return "INIT_FAILED";
        default: return "UNKNOWN";
    }
}

//=============================================================================
// Constructor/Initializer Entry Structure
//=============================================================================

struct InitFunctionEntry {
    std::string name;           // Function name if available
    uint64_t address;           // Function address
    std::string module;         // Module containing this function
    InitStage stage;            // Which stage this belongs to
    
    // Timing
    Timestamp start_time;
    Timestamp end_time;
    Duration duration;
    
    // Status
    bool completed{false};
    bool crashed{false};
    int exit_code{0};           // If applicable
    
    // Context
    std::string error_message;
    std::vector<uint64_t> called_functions;  // Functions called during this init
};

//=============================================================================
// Destructor Registration Entry
//=============================================================================

struct DestructorEntry {
    uint64_t destructor_addr;
    std::string name;
    std::string object_type;    // "atexit", "__cxa_atexit", "tls", etc.
    void* object_ptr;           // Object to destroy (if applicable)
    Timestamp registration_time;
    int priority;               // Execution order priority (lower = earlier)
    bool executed{false};
    Timestamp execution_time;
};

//=============================================================================
// CRT Initialization Tracking
//=============================================================================

struct CRTInitState {
    bool atexit_setup{false};
    bool malloc_initialized{false};
    bool signal_handlers_set{false};
    bool errno_setup{false};
    bool locale_initialized{false};
    bool heap_created{false};
    
    std::map<std::string, bool> subsystems;
    
    double completion_percentage() const {
        int total = 7 + static_cast<int>(subsystems.size());
        int done = (atexit_setup ? 1 : 0) +
                   (malloc_initialized ? 1 : 0) +
                   (signal_handlers_set ? 1 : 0) +
                   (errno_setup ? 1 : 0) +
                   (locale_initialized ? 1 : 0) +
                   (heap_created ? 1 : 0);
        for (const auto& [name, ready] : subsystems) {
            if (ready) done++;
        }
        return total > 0 ? (100.0 * done / total) : 100.0;
    }
};

//=============================================================================
// Complete Init Timeline Report
//=============================================================================

struct InitTimelineReport {
    // Overall status
    InitStage current_stage{InitStage::NOT_STARTED};
    InitStage final_stage{InitStage::NOT_STARTED};
    bool completed_successfully{false};
    
    // Timeline
    struct StageTimestamp {
        InitStage stage;
        Timestamp entry_time;
        Timestamp exit_time;
        Duration duration;
        bool completed;
    };
    std::vector<StageTimestamp> stage_timeline;
    
    // Individual functions executed
    std::vector<InitFunctionEntry> init_functions;
    
    // Destructors registered
    std::vector<DestructorEntry> destructors;
    
    // CRT state
    CRTInitState crt_state;
    
    // Failure analysis (if failed)
    struct FailureInfo {
        InitStage failure_stage;
        InitFunctionEntry* failing_function{nullptr};
        std::string reason;
        Timestamp failure_time;
        std::string crash_snapshot_path;
        
        // Suggested fix
        std::string suggested_investigation;
        std::vector<std::string> related_events;
    } failure_info;
    
    // Statistics
    size_t total_init_functions{0};
    size_t completed_functions{0};
    size_t failed_functions{0};
    Duration total_init_time{0};
    
    // Serialization
    std::string to_json() const;
    std::string to_markdown() const;
};

//=============================================================================
// Runtime Init Trace Plugin Implementation
//=============================================================================

class RuntimeInitTracePlugin : public DiagnosticPlugin {
public:
    RuntimeInitTracePlugin() = default;
    ~RuntimeInitTracePlugin() override { shutdown(); }
    
    // Identity
    std::string name() const override { return "runtime_init_trace"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "Tracks runtime initialization sequence (DT_INIT, constructors, CRT setup)";
    }
    
    // Lifecycle
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        reset_internal();
        active_ = true;
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return;
        
        // Generate final report before shutdown
        generate_report();
        active_ = false;
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        reset_internal();
    }
    
    //=========================================================================
    // Stage Transition Methods
    //=========================================================================
    
    /**
     * @brief Begin an initialization stage
     * @param stage The stage entering
     * @return true if valid transition
     */
    bool begin_stage(InitStage stage) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!validate_transition(current_stage_, stage)) {
            emit_warning("Invalid init stage transition from " + 
                        std::string(init_stage_to_string(current_stage_)) + 
                        " to " + std::string(init_stage_to_string(stage)));
            return false;
        }
        
        current_stage_ = stage;
        
        InitTimelineReport::StageTimestamp ts;
        ts.stage = stage;
        ts.entry_time = now();
        ts.completed = false;
        
        stage_timeline_.push_back(ts);
        
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "INIT_STAGE_BEGIN";
        event.severity = Severity::INFO;
        event.message = std::string("Entered initialization stage: ") + init_stage_to_string(stage);
        event.metadata["stage"] = init_stage_to_string(stage);
        event.metadata["previous_stage"] = init_stage_to_string(
            stage_timeline_.size() > 1 ? 
            stage_timeline_[stage_timeline_.size() - 2].stage : InitStage::NOT_STARTED);
        emit_event(event);
        
        return true;
    }
    
    /**
     * @brief Complete the current initialization stage
     */
    bool end_current_stage(bool success = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (stage_timeline_.empty()) return false;
        
        auto& last = stage_timeline_.back();
        last.exit_time = now();
        last.duration = std::chrono::duration_cast<Duration>(last.exit_time - last.entry_time);
        last.completed = success;
        
        if (!success) {
            current_stage_ = InitStage::INIT_FAILED;
            report_.final_stage = InitStage::INIT_FAILED;
            report_.failure_info.failure_stage = last.stage;
            report_.failure_info.failure_time = now();
            report_.failure_info.reason = "Stage did not complete successfully";
            
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "INIT_STAGE_FAILED";
            event.severity = Severity::ERROR;
            event.message = "Initialization stage failed: " + std::string(init_stage_to_string(last.stage));
            emit_event(event);
        } else {
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "INIT_STAGE_COMPLETE";
            event.severity = Severity::DEBUG;
            event.message = std::string("Completed stage: ") + init_stage_to_string(last.stage) + 
                           " in " + std::to_string(last.duration.count() / 1000.0) + "ms";
            event.numeric_data["duration_us"] = last.duration.count();
            emit_event(event);
        }
        
        return true;
    }
    
    //=========================================================================
    // Init Function Tracking
    //=========================================================================
    
    /**
     * @brief Record that an init function is about to execute
     * @return Function entry ID for matching with complete_function()
     */
    size_t begin_init_function(uint64_t address, const std::string& name = "", 
                               const std::string& module = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        InitFunctionEntry entry;
        entry.name = name.empty() ? "func_0x" + to_hex_string(address) : name;
        entry.address = address;
        entry.module = module;
        entry.stage = current_stage_;
        entry.start_time = now();
        
        init_functions_.push_back(entry);
        report_.total_init_functions++;
        
        return init_functions_.size() - 1;
    }
    
    /**
     * @brief Mark an init function as completed
     */
    void complete_init_function(size_t id, bool success = true, int exit_code = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (id >= init_functions_.size()) return;
        
        auto& func = init_functions_[id];
        func.end_time = now();
        func.duration = std::chrono::duration_cast<Duration>(func.end_time - func.start_time);
        func.completed = success;
        func.exit_code = exit_code;
        
        if (success) {
            report_.completed_functions++;
        } else {
            func.crashed = true;
            report_.failed_functions++;
            
            // Update failure info
            report_.failure_info.failing_function = &func;
            report_.failure_info.failure_stage = func.stage;
            report_.failure_info.failure_time = func.end_time;
            report_.failure_info.reason = "Initialization function crashed or failed: " + func.name;
            report_.failure_info.suggested_investigation = 
                "Investigate " + func.name + " at address 0x" + to_hex_string(func.address) +
                ". Check for null pointer dereference, uninitialized static variables, "
                "or dependency on other modules not yet initialized.";
            
            current_stage_ = InitStage::INIT_FAILED;
            
            DiagnosticEvent event;
            event.source_plugin = name();
            event.event_type = "INIT_FUNCTION_FAILED";
            event.severity = Severity::CRITICAL;
            event.message = "Init function failed: " + func.name;
            event.metadata["function"] = func.name;
            event.metadata["address"] = to_hex_string(func.address);
            event.metadata["stage"] = init_stage_to_string(func.stage);
            emit_event(event);
        }
        
        report_.total_init_time += func.duration;
    }
    
    //=========================================================================
    // Destructor Registration
    //=========================================================================
    
    /**
     * @brief Register a destructor for later execution
     */
    void register_destructor(uint64_t addr, const std::string& name,
                            const std::string& type, void* obj = nullptr,
                            int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        DestructorEntry entry;
        entry.destructor_addr = addr;
        entry.name = name.empty() ? "destructor_0x" + to_hex_string(addr) : name;
        entry.object_type = type;
        entry.object_ptr = obj;
        entry.registration_time = now();
        entry.priority = priority;
        
        destructors_.push_back(entry);
    }
    
    //=========================================================================
    // CRT State Tracking
    //=========================================================================
    
    void set_crt_subsystem_ready(const std::string& subsystem, bool ready = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (subsystem == "atexit") crt_state_.atexit_setup = ready;
        else if (subsystem == "malloc") crt_state_.malloc_initialized = ready;
        else if (subsystem == "signals") crt_state_.signal_handlers_set = ready;
        else if (subsystem == "errno") crt_state_.errno_setup = ready;
        else if (subsystem == "locale") crt_state_.locale_initialized = ready;
        else if (subsystem == "heap") crt_state_.heap_created = ready;
        else crt_state_.subsystems[subsystem] = ready;
    }
    
    //=========================================================================
    // Reporting
    //=========================================================================
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return report_.to_json();
    }
    
    const InitTimelineReport& get_full_report() const {
        std::lock_guard<std::mutex> lock(mutex_);
        update_report();
        return report_;
    }
    
    /**
     * @brief Generate human-readable timeline in markdown format
     */
    std::string generate_timeline_markdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        update_report();
        return report_.to_markdown();
    }
    
    /**
     * @brief Quick status string for logging
     */
    std::string get_status_string() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "RuntimeInitTrace[stage=" << init_stage_to_string(current_stage_)
            << ", functions=" << report_.completed_functions << "/" << report_.total_init_functions
            << ", failed=" << report_.failed_functions
            << ", crt=" << std::fixed << std::setprecision(1) << crt_state_.completion_percentage() << "%]";
        return oss.str();
    }
    
    // Event handling
    void on_event(const DiagnosticEvent& event) override {
        // Listen for crash events to correlate with init state
        if (event.event_type.find("CRASH") != std::string::npos ||
            event.event_type.find("SIGSEGV") != std::string::npos ||
            event.event_type.find("SIGABRT") != std::string::npos) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (current_stage_ != InitStage::INIT_COMPLETE && 
                current_stage_ != InitStage::NOT_STARTED) {
                report_.failure_info.failure_stage = current_stage_;
                report_.failure_info.failure_time = event.timestamp;
                
                // Add crash as related event
                report_.failure_info.related_events.push_back(event.event_id);
            }
        }
    }

private:
    //=========================================================================
    // Internal State
    //=========================================================================
    
    InitStage current_stage_{InitStage::NOT_STARTED};
    std::vector<InitTimelineReport::StageTimestamp> stage_timeline_;
    std::vector<InitFunctionEntry> init_functions_;
    std::vector<DestructorEntry> destructors_;
    CRTInitState crt_state_;
    
    mutable InitTimelineReport report_;  // Mutable for lazy generation in const methods
    
    //=========================================================================
    // Helper Methods
    //=========================================================================
    
    void reset_internal() {
        current_stage_ = InitStage::NOT_STARTED;
        stage_timeline_.clear();
        init_functions_.clear();
        destructors_.clear();
        crt_state_ = CRTInitState{};
        report_ = InitTimelineReport{};
    }
    
    mutable void update_report() const {
        report_.current_stage = current_stage_;
        report_.final_stage = current_stage_;  // Will be updated if failed
        report_.stage_timeline = stage_timeline_;
        report_.init_functions = init_functions_;
        report_.destructors = destructors_;
        report_.crt_state = crt_state_;
        report_.completed_successfully = (current_stage_ == InitStage::INIT_COMPLETE);
    }
    
    bool validate_transition(InitStage from, InitStage to) const {
        // Define valid transitions
        switch (from) {
            case InitStage::NOT_STARTED:
                return to == InitStage::ELF_DYNAMIC_LINKING;
            case InitStage::ELF_DYNAMIC_LINKING:
                return to == InitStage::DT_INIT || to == InitStage::DT_INIT_ARRAY_PREPARE;
            case InitStage::DT_INIT:
                return to == InitStage::DT_INIT_ARRAY_PREPARE || to == InitStage::CPP_CONSTRUCTORS;
            case InitStage::DT_INIT_ARRAY_PREPARE:
                return to == InitStage::DT_INIT_ARRAY_EXECUTING;
            case InitStage::DT_INIT_ARRAY_EXECUTING:
                return to == InitStage::DT_SCE_INIT_ARRAY || to == InitStage::CPP_CONSTRUCTORS;
            case InitStage::DT_SCE_INIT_ARRAY:
                return to == InitStage::CPP_CONSTRUCTORS;
            case InitStage::CPP_CONSTRUCTORS:
                return to == InitStage::TLS_SETUP || to == InitStage::CRT_INIT;
            case InitStage::TLS_SETUP:
                return to == InitStage::CRT_INIT;
            case InitStage::CRT_INIT:
                return to == InitStage::LIBRARY_INITIALIZERS;
            case InitStage::LIBRARY_INITIALIZERS:
                return to == InitStage::PRE_MAIN;
            case InitStage::PRE_MAIN:
                return to == InitStage::MAIN_ENTRY;
            case InitStage::MAIN_ENTRY:
                return to == InitStage::INIT_COMPLETE;
            case InitStage::INIT_COMPLETE:
                return false;  // Terminal state
            case InitStage::INIT_FAILED:
                return false;  // Terminal state
            default:
                return false;
        }
    }
    
    void emit_warning(const std::string& msg) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "INIT_WARNING";
        event.severity = Severity::WARNING;
        event.message = msg;
        emit_event(event);
    }
    
    static std::string to_hex_string(uint64_t val) {
        std::ostringstream oss;
        oss << std::hex << val;
        return oss.str();
    }
};

//=============================================================================
// Inline Implementations of Report Serialization
//=============================================================================

inline std::string InitTimelineReport::to_json() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"current_stage\": \"" << init_stage_to_string(current_stage) << "\",\n";
    json << "  \"final_stage\": \"" << init_stage_to_string(final_stage) << "\",\n";
    json << "  \"completed_successfully\": " << (completed_successfully ? "true" : "false") << ",\n";
    
    // Stage timeline
    json << "  \"stages\": [\n";
    for (size_t i = 0; i < stage_timeline.size(); i++) {
        const auto& s = stage_timeline[i];
        json << "    {\n";
        json << "      \"stage\": \"" << init_stage_to_string(s.stage) << "\",\n";
        json << "      \"entry_time_ms\": " << timestamp_to_ms(s.entry_time) << ",\n";
        json << "      \"duration_us\": " << s.duration.count() << ",\n";
        json << "      \"completed\": " << (s.completed ? "true" : "false") << "\n";
        json << "    }" << (i < stage_timeline.size() - 1 ? "," : "") << "\n";
    }
    json << "  ],\n";
    
    // Functions
    json << "  \"init_functions\": [\n";
    for (size_t i = 0; i < init_functions.size(); i++) {
        const auto& f = init_functions[i];
        json << "    {\n";
        json << "      \"name\": \"" << f.name << "\",\n";
        json << "      \"address\": \"0x" << std::hex << f.address << std::dec << "\",\n";
        json << "      \"module\": \"" << f.module << "\",\n";
        json << "      \"stage\": \"" << init_stage_to_string(f.stage) << "\",\n";
        json << "      \"duration_us\": " << f.duration.count() << ",\n";
        json << "      \"completed\": " << (f.completed ? "true" : "false") << ",\n";
        json << "      \"crashed\": " << (f.crashed ? "true" : "false") << "\n";
        json << "    }" << (i < init_functions.size() - 1 ? "," : "") << "\n";
    }
    json << "  ],\n";
    
    // Statistics
    json << "  \"statistics\": {\n";
    json << "    \"total_functions\": " << total_init_functions << ",\n";
    json << "    \"completed\": " << completed_functions << ",\n";
    json << "    \"failed\": " << failed_functions << ",\n";
    json << "    \"total_time_us\": " << total_init_time.count() << "\n";
    json << "  },\n";
    
    // CRT state
    json << "  \"crt_completion_pct\": " << std::fixed << std::setprecision(1) 
        << crt_state.completion_percentage() << "\n";
    
    // Failure info (if any)
    if (failure_info.failure_stage != InitStage::NOT_STARTED) {
        json << ",\n  \"failure\": {\n";
        json << "    \"stage\": \"" << init_stage_to_string(failure_info.failure_stage) << "\",\n";
        json << "    \"reason\": \"" << failure_info.reason << "\",\n";
        json << "    \"suggested_investigation\": \"" << failure_info.suggested_investigation << "\"\n";
        json << "  }\n";
    } else {
        json << "\n";
    }
    
    json << "}\n";
    return json.str();
}

inline std::string InitTimelineReport::to_markdown() const {
    std::ostringstream md;
    
    md << "# Runtime Initialization Timeline Report\n\n";
    md << "**Status**: " << (completed_successfully ? "✅ COMPLETE" : "❌ FAILED") << "\n\n";
    md << "**Current Stage**: `" << init_stage_to_string(current_stage) << "`\n\n";
    
    // Timeline table
    md << "## Stage Timeline\n\n";
    md << "| Stage | Duration | Status |\n";
    md << "|-------|----------|--------|\n";
    
    for (const auto& s : stage_timeline) {
        md << "`" << init_stage_to_string(s.stage) << "` | ";
        if (s.duration.count() > 0) {
            md << (s.duration.count() / 1000.0) << " ms";
        } else {
            md << "*in progress*";
        }
        md << " | " << (s.completed ? "✅" : "❌") << " |\n";
    }
    
    // Functions summary
    if (!init_functions.empty()) {
        md << "\n## Initialization Functions\n\n";
        md << "- **Total**: " << total_init_functions << "\n";
        md << "- **Completed**: " << completed_functions << "\n";
        md << "- **Failed**: " << failed_functions << "\n";
        
        if (failed_functions > 0) {
            md << "\n### Failed Functions\n\n";
            for (const auto& f : init_functions) {
                if (!f.completed || f.crashed) {
                    md << "- **" << f.name << "** (`0x" << std::hex << f.address << std::dec << "`) ";
                    md << "in stage `" << init_stage_to_string(f.stage) << "`\n";
                    if (!f.error_message.empty()) {
                        md << "  - Error: " << f.error_message << "\n";
                    }
                }
            }
        }
    }
    
    // CRT State
    md << "\n## CRT Initialization\n\n";
    md << "**Completion**: " << std::fixed << std::setprecision(1) 
       << crt_state.completion_percentage() << "%\n\n";
    
    md << "| Subsystem | Ready |\n";
    md("|-----------|--------|\n";
    md << "| atexit | " << (crt_state.atexit_setup ? "✅" : "⏳") << " |\n";
    md << "| malloc | " << (crt_state.malloc_initialized ? "✅" : "⏳") << " |\n";
    md << "| signals | " << (crt_state.signal_handlers_set ? "✅" : "⏳") << " |\n";
    md << "| heap | " << (crt_state.heap_created ? "✅" : "⏳") << " |\n";
    
    // Failure analysis
    if (failure_info.failure_stage != InitStage::NOT_STARTED) {
        md << "\n## ⚠️ Failure Analysis\n\n";
        md << "**Failed Stage**: `" << init_stage_to_string(failure_info.failure_stage) << "`\n\n";
        md << "**Reason**: " << failure_info.reason << "\n\n";
        
        if (failure_info.failing_function) {
            md << "**Failing Function**: `" << failure_info.failing_function->name << "`\n\n";
        }
        
        if (!failure_info.suggested_investigation.empty()) {
            md << "**Suggested Investigation**:\n" << failure_info.suggested_investigation << "\n\n";
        }
    }
    
    return md.str();
}

} // namespace diagnostics
} // namespace prosper
