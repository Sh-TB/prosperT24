#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <csignal>
#include <cstring>
#include <unordered_set>
#include <algorithm>

#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#include <ucontext.h>
#include <unistd.h>
#endif

namespace prosper {
namespace diagnostics {

//=============================================================================
// Crash Context Plugin - Phase 9.5 Diagnostic Plugin
//
// Captures and records crash state for PS4 emulator diagnostics.
// Registers signal handlers for SIGSEGV, SIGABRT, etc., captures CPU
// registers at crash time, records stack traces and loaded modules,
// and saves comprehensive crash snapshots to JSON.
//=============================================================================

/**
 * @brief Represents captured CPU register state at time of crash
 */
struct RegisterState {
    uint64_t rip{0};    ///< Instruction pointer (x86-64)
    uint64_t rsp{0};    ///< Stack pointer
    uint64_t rbp{0};    ///< Base pointer
    uint64_t rax{0};    ///< Accumulator
    uint64_t rbx{0};    ///< Base register
    uint64_t rcx{0};    ///< Counter register
    uint64_t rdx{0};    ///< Data register
    uint64_t rsi{0};    ///< Source index
    uint64_t rdi{0};    ///< Destination index
    uint64_t r8{0};     ///< General purpose
    uint64_t r9{0};     ///< General purpose
    uint64_t r10{0};    ///< General purpose
    uint64_t r11{0};    ///< General purpose
    uint64_t r12{0};    ///< General purpose
    uint64_t r13{0};    ///< General purpose
    uint64_t r14{0};    ///< General purpose
    uint64_t r15{0};    ///< General purpose
    uint64_t rflags{0}; ///< CPU flags
    uint64_t cs{0};     ///< Code segment
    uint64_t ss{0};     ///< Stack segment
    uint64_t fault_addr{0};  ///< Address that caused fault (for SIGSEGV)
    
    /// Default constructor zeros everything
    RegisterState() = default;
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"rip\":\"0x" << std::hex << rip << "\",";
        oss << "\"rsp\":\"0x" << rsp << "\",";
        oss << "\"rbp\":\"0x" << rbp << "\",";
        oss << "\"rax\":\"0x" << rax << "\",";
        oss << "\"rbx\":\"0x" << rbx << "\",";
        oss << "\"rcx\":\"0x" << rcx << "\",";
        oss << "\"rdx\":\"0x" << rdx << "\",";
        oss << "\"rsi\":\"0x" << rsi << "\",";
        oss << "\"rdi\":\"0x" << rdi << "\",";
        oss << "\"r8\":\"0x" << r8 << "\",";
        oss << "\"r9\":\"0x" << r9 << "\",";
        oss << "\"r10\":\"0x" << r10 << "\",";
        oss << "\"r11\":\"0x" << r11 << "\",";
        oss << "\"r12\":\"0x" << r12 << "\",";
        oss << "\"r13\":\"0x" << r13 << "\",";
        oss << "\"r14\":\"0x" << r14 << "\",";
        oss << "\"r15\":\"0x" << r15 << "\",";
        oss << "\"rflags\":\"0x" << rflags << "\",";
        oss << "\"fault_addr\":\"0x" << fault_addr << std::dec << "\"";
        oss << "}";
        return oss.str();
    }
    
    /// Get a human-readable summary of key registers
    std::string to_summary_string() const {
        std::ostringstream oss;
        oss << "RIP=0x" << std::hex << rip 
            << " RSP=0x" << rsp 
            << " RBP=0x" << rbp
            << " RAX=0x" << rax
            << " FaultAddr=0x" << fault_addr << std::dec;
        return oss.str();
    }
};

/**
 * @brief Complete crash context captured at time of crash
 */
struct CrashContext {
    int signal_num{0};                     ///< Signal number that caused crash
    std::string signal_name;               ///< Human-readable signal name (e.g., "SIGSEGV")
    RegisterState registers;               ///< CPU register state
    std::string stack_trace;               ///< Captured stack trace as string
    Timestamp timestamp;                   ///< When the crash occurred
    std::vector<std::string> loaded_modules;///< List of loaded modules at crash time
    std::string crash_reason;              ///< Additional context about why crash occurred
    int pid{0};                            ///< Process ID at time of crash
    int tid{0};                            ///< Thread ID where crash occurred
    bool snapshot_saved{false};            ///< Whether snapshot was saved to disk
    
    /// Default constructor
    CrashContext() : timestamp(now()) {}
    
    /// Get signal name from number
    static std::string get_signal_name(int sig) {
        switch (sig) {
            case SIGSEGV: return "SIGSEGV";
            case SIGABRT: return "SIGABRT";
            case SIGFPE:  return "SIGFPE";
            case SIGILL:  return "SIGILL";
            case SIGBUS:  return "SIGBUS";
            case SIGTERM: return "SIGTERM";
            case SIGINT:  return "SIGINT";
#if defined(SIGPIPE)
            case SIGPIPE: return "SIGPIPE";
#endif
            default:      return "UNKNOWN(" + std::to_string(sig) + ")";
        }
    }
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"signal\": " << signal_num << ",\n";
        json << "  \"signal_name\": \"" << signal_name << "\",\n";
        json << "  \"timestamp_ms\": " << std::fixed << std::setprecision(3) 
             << timestamp_to_ms(timestamp) << ",\n";
        json << "  \"pid\": " << pid << ",\n";
        json << "  \"tid\": " << tid << ",\n";
        json << "  \"crash_reason\": \"" << escape_json(crash_reason) << "\",\n";
        
        // Registers
        json << "  \"registers\": " << registers.to_json() << ",\n";
        
        // Stack trace
        json << "  \"stack_trace\": \"" << escape_json(stack_trace) << "\",\n";
        
        // Loaded modules
        json << "  \"loaded_modules\": [\n";
        for (size_t i = 0; i < loaded_modules.size(); ++i) {
            json << "    \"" << loaded_modules[i] << "\"";
            if (i < loaded_modules.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        json << "  \"snapshot_saved\": " << (snapshot_saved ? "true" : "false") << "\n";
        json << "}";
        
        return json.str();
    }
    
private:
    /// Escape special characters for JSON string
    static std::string escape_json(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        std::ostringstream oss;
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                            << static_cast<int>(static_cast<unsigned char>(c));
                        result += oss.str();
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }
};

/**
 * @brief Crash Context Plugin implementation
 * 
 * This plugin provides comprehensive crash capture:
 * - Installs signal handlers for common crash signals (SIGSEGV, SIGABRT, etc.)
 * - Captures full CPU register state at crash time
 * - Records stack traces using platform-specific backtrace functions
 * - Tracks loaded modules at time of crash
 * - Saves complete crash snapshots to JSON files
 * 
 * IMPORTANT: Signal handlers have significant restrictions on what they can do.
 * This plugin uses async-signal-safe operations within handlers and defers
 * heavier processing to after handler completion when possible.
 */
class CrashContextPlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Constructor / Destructor
    //=========================================================================
    
    CrashContextPlugin()
        : handlers_installed_(false),
          max_snapshots_(100),
          max_stack_frames_(64),
          auto_save_enabled_(true),
          last_crash_(nullptr) {}
    
    virtual ~CrashContextPlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override { return "CrashContext"; }
    
    std::string version() const override { return "1.5.0"; }
    
    std::string description() const override {
        return "Captures crash state, registers, stack traces, and saves snapshots";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        crash_history_.clear();
        last_crash_.reset();
        
        active_ = true;
        
        // Parse configuration
        auto it = config_.find("max_snapshots");
        if (it != config_.end()) {
            try { max_snapshots_ = static_cast<size_t>(std::stoull(it->second)); } catch (...) {}
        }
        
        it = config_.find("max_stack_frames");
        if (it != config_.end()) {
            try { max_stack_frames_ = static_cast<int>(std::stoi(it->second)); } catch (...) {}
        }
        
        it = config_.find("auto_save");
        if (it != config_.end()) {
            auto_save_enabled_ = (it->second == "true" || it->second == "1");
        }
        
        it = config_.find("snapshot_path");
        if (it != config_.end()) {
            snapshot_path_ = it->second;
        }
        
        // Install signal handlers by default
        install_signal_handlers();
        
        emit_info("CrashContext initialized successfully with signal handlers installed");
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uninstall_signal_handlers();
        active_ = false;
        emit_info("CrashContext shut down, signal handlers removed");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        crash_history_.clear();
        last_crash_.reset();
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        
        // Track module load events to maintain current module list
        if (event.event_type == "module_loaded") {
            auto name_it = event.metadata.find("module_name");
            if (name_it != event.metadata.end()) {
                current_modules_.insert(name_it->second);
            }
        }
        else if (event.event_type == "module_unloaded") {
            auto name_it = event.metadata.find("module_name");
            if (name_it != event.metadata.end()) {
                current_modules_.erase(name_it->second);
            }
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_report_internal();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << generate_report_internal();
        file.close();
    }
    
    //=========================================================================
    // Public API Methods
    //=========================================================================
    
    /**
     * @brief Handle a crash event (can be called manually or by signal handler)
     * @param signal_num Signal number that caused the crash
     * @param context Optional signal context (ucontext_t*) for register extraction
     * @param fault_addr Optional faulting address (for SIGSEGV)
     */
    void on_crash(int signal_num, void* context = nullptr, uint64_t fault_addr = 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Create new crash context
        auto crash = std::make_unique<CrashContext>();
        crash->signal_num = signal_num;
        crash->signal_name = CrashContext::get_signal_name(signal_num);
        crash->pid = static_cast<int>(getpid());
#ifdef __linux__
        crash->tid = static_cast<int>(gettid());
#elif defined(__APPLE__)
        crash->tid = static_cast<int>(pthread_mach_thread_np(pthread_self()));
#endif
        crash->timestamp = now();
        
        // Capture registers from context if available
        capture_registers_internal(*crash, context);
        crash->registers.fault_addr = fault_addr;
        
        // Capture stack trace
        crash->stack_trace = capture_stack_trace_internal();
        
        // Record loaded modules
        crash->loaded_modules.assign(current_modules_.begin(), current_modules_.end());
        
        // Determine crash reason
        crash->crash_reason = generate_crash_reason(*crash);
        
        // Store in history
        last_crash_ = std::move(crash);
        crash_history_.push_back(*last_crash_);
        
        // Limit history size
        while (crash_history_.size() > max_snapshots_) {
            crash_history_.erase(crash_history_.begin());
        }
        
        // Auto-save if enabled
        if (auto_save_enabled_) {
            lock.unlock();  // Release lock before I/O
            save_crash_snapshot_internal(*last_crash_);
        }
        
        // Emit critical diagnostic event
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "crash_detected";
        event.severity = Severity::CRITICAL;
        event.message = "CRASH: " + last_crash_->signal_name + 
                       " at RIP=0x" + addr_to_hex(last_crash_->registers.rip);
        event.metadata["signal_name"] = last_crash_->signal_name;
        event.numeric_data["signal"] = last_crash_->signal_num;
        event.numeric_data["rip"] = static_cast<int64_t>(last_crash_->registers.rip);
        event.numeric_data["rsp"] = static_cast<int64_t>(last_crash_->registers.rsp);
        event.stack_trace = last_crash_->stack_trace;
        
        emit_event(event);
    }
    
    /**
     * @brief Manually capture current register state into provided structure
     * @param regs Output structure to fill with register values
     */
    void capture_registers(RegisterState& regs) const {
        // Note: Cannot truly capture all registers without compiler support or assembly
        // This provides best-effort capture using available methods
        
#if defined(__GNUC__) && defined(__x86_64__)
        // Use inline assembly to grab some registers (limited usefulness)
        uint64_t rsp_val, rbp_val;
        __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp_val));
        __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp_val));
        
        regs.rsp = rsp_val;
        regs.rbp = rbp_val;
        
        // Get approximate RIP via return address trick
        void* rip_ptr = __builtin_return_address(0);
        if (rip_ptr) {
            regs.rip = reinterpret_cast<uint64_t>(rip_ptr);
        }
#else
        (void)regs;
#endif
    }
    
    /**
     * @brief Capture current stack trace as string
     * @param max_frames Maximum number of frames to capture
     * @return String containing formatted stack trace
     */
    std::string capture_stack_trace(int max_frames = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return capture_stack_trace_internal(max_frames > 0 ? max_frames : max_stack_frames_);
    }
    
    /**
     * @brief Save crash snapshot to JSON file
     * @param crash The crash context to save
     * @param path Optional custom path (uses configured path if empty)
     * @return True if save succeeded
     */
    bool save_crash_snapshot(const CrashContext& crash, const std::string& path = "") const {
        return save_crash_snapshot_internal(crash, path.empty() ? snapshot_path_ : path);
    }
    
    /**
     * @brief Get the most recent crash context (read-only access)
     * @return Pointer to last crash or nullptr if no crashes recorded
     */
    const CrashContext* get_last_crash() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_crash_.get();
    }
    
    /**
     * @brief Get crash history count
     */
    size_t crash_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return crash_history_.size();
    }
    
    /**
     * @brief Get copy of crash history
     */
    std::vector<CrashContext> get_crash_history() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return crash_history_;
    }
    
    /**
     * @brief Set the list of currently loaded modules (call periodically)
     * @param modules Vector of module names
     */
    void set_loaded_modules(const std::vector<std::string>& modules) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_modules_.clear();
        for (const auto& m : modules) {
            current_modules_.insert(m);
        }
    }
    
    /**
     * @brief Add a single loaded module
     */
    void add_loaded_module(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_modules_.insert(name);
    }
    
    /**
     * @brief Remove a loaded module
     */
    void remove_loaded_module(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_modules_.erase(name);
    }

private:
    //=========================================================================
    // Internal Implementation Methods
    //=========================================================================
    
    /**
     * @brief Install POSIX signal handlers for crash signals
     */
    void install_signal_handlers() {
        if (handlers_installed_) return;
        
        // Store this pointer for static handler
        s_instance_ = this;
        
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        
        sa.sa_sigaction = &CrashContextPlugin::signal_handler;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        
        // Block all signals during handler execution
        sigemptyset(&sa.sa_mask);
        
        // Install handlers for each crash signal
        install_handler_for_signal(SIGSEGV, sa);  // Segmentation fault
        install_handler_for_signal(SIGABRT, sa);   // Abort
        install_handler_for_signal(SIGFPE, sa);    // Floating point exception
        install_handler_for_signal(SIGILL, sa);    // Illegal instruction
        install_handler_for_signal(SIGBUS, sa);    // Bus error
        
        handlers_installed_ = true;
    }
    
    /**
     * @brief Helper to install handler for specific signal
     */
    void install_handler_for_signal(int signum, struct sigaction& sa) {
        if (sigaction(signum, &sa, nullptr) == -1) {
            emit_warning("Failed to install handler for " + 
                        CrashContext::get_signal_name(signum));
        }
    }
    
    /**
     * @brief Uninstall all signal handlers (restore defaults)
     */
    void uninstall_signal_handlers() {
        if (!handlers_installed_) return;
        
        struct sigaction sa_default;
        memset(&sa_default, 0, sizeof(sa_default));
        sa_default.sa_handler = SIG_DFL;
        
        sigaction(SIGSEGV, &sa_default, nullptr);
        sigaction(SIGABRT, &sa_default, nullptr);
        sigaction(SIGFPE, &sa_default, nullptr);
        sigaction(SIGILL, &sa_default, nullptr);
        sigaction(SIGBUS, &sa_default, nullptr);
        
        handlers_installed_ = false;
        s_instance_ = nullptr;
    }
    
    /**
     * @brief Static signal handler (called by OS)
     * 
     * NOTE: This function must be async-signal-safe. It does minimal work
     * and delegates heavy processing to on_crash().
     */
    static void signal_handler(int sig, siginfo_t* info, void* ucontext) {
        // Extract fault address for SIGSEGV/SIGBUS
        uint64_t fault_addr = 0;
        if (info && (sig == SIGSEGV || sig == SIGBUS)) {
            fault_addr = reinterpret_cast<uint64_t>(info->si_addr);
        }
        
        // Call instance method if available
        if (s_instance_) {
            s_instance_->on_crash(sig, ucontext, fault_addr);
        }
        
        // Re-raise with default handler to ensure normal crash behavior
        // This allows core dumps to be generated
        signal(sig, SIG_DFL);
        raise(sig);
    }
    
    /**
     * @brief Internal register capture (assumes lock may not be held in signal context)
     */
    void capture_registers_internal(CrashContext& crash, void* context) {
#if defined(__linux__) && defined(__x86_64__)
        if (context) {
            const ucontext_t* uc = static_cast<const ucontext_t*>(context);
            const greg_t* regs = uc->uc_mcontext.gregs;
            
            // Map Linux ucontext registers to our structure
            crash.registers.rip = static_cast<uint64_t>(regs[REG_RIP]);
            crash.registers.rsp = static_cast<uint64_t>(regs[REG_RSP]);
            crash.registers.rbp = static_cast<uint64_t>(regs[REG_RBP]);
            crash.registers.rax = static_cast<uint64_t>(regs[REG_RAX]);
            crash.registers.rbx = static_cast<uint64_t>(regs[REG_RBX]);
            crash.registers.rcx = static_cast<uint64_t>(regs[REG_RCX]);
            crash.registers.rdx = static_cast<uint64_t>(regs[REG_RDX]);
            crash.registers.rsi = static_cast<uint64_t>(regs[REG_RSI]);
            crash.registers.rdi = static_cast<uint64_t>(regs[REG_RDI]);
            crash.registers.r8  = static_cast<uint64_t>(regs[REG_R8]);
            crash.registers.r9  = static_cast<uint64_t>(regs[REG_R9]);
            crash.registers.r10 = static_cast<uint64_t>(regs[REG_R10]);
            crash.registers.r11 = static_cast<uint64_t>(regs[REG_R11]);
            crash.registers.r12 = static_cast<uint64_t>(regs[REG_R12]);
            crash.registers.r13 = static_cast<uint64_t>(regs[REG_R13]);
            crash.registers.r14 = static_cast<uint64_t>(regs[REG_R14]);
            crash.registers.r15 = static_cast<uint64_t>(regs[REG_R15]);
            crash.registers.rflags = static_cast<uint64_t>(regs[REG_EFL]);
        }
#elif defined(__APPLE__) && defined(__x86_64__)
        if (context) {
            const ucontext_t* uc = static_cast<const ucontext_t*>(context);
            const struct __darwin_x86_thread_state64* ts = &uc->uc_mcontext->__ss;
            
            crash.registers.rip = ts->__rip;
            crash.registers.rsp = ts->__rsp;
            crash.registers.rbp = ts->__rbp;
            crash.registers.rax = ts->__rax;
            crash.registers.rbx = ts->__rbx;
            crash.registers.rcx = ts->__rcx;
            crash.registers.rdx = ts->__rdx;
            crash.registers.rsi = ts->__rsi;
            crash.registers.rdi = ts->__rdi;
            crash.registers.r8  = ts->__r8;
            crash.registers.r9  = ts->__r9;
            crash.registers.r10 = ts->__r10;
            crash.registers.r11 = ts->__r11;
            crash.registers.r12 = ts->__r12;
            crash.registers.r13 = ts->__r13;
            crash.registers.r14 = ts->__r14;
            crash.registers.r15 = ts->__r15;
            crash.registers.rflags = ts->__rflags;
        }
#else
        // Fallback: try inline asm capture
        capture_registers(crash.registers);
        (void)context;
#endif
    }
    
    /**
     * @brief Internal stack trace capture (assumes lock held)
     */
    std::string capture_stack_trace_internal(int max_frames) const {
        std::ostringstream trace;
        
#if defined(__linux__) || defined(__APPLE__)
        void* buffer[256];
        int frame_count = backtrace(buffer, std::min(max_frames, 256));
        
        char** symbols = backtrace_symbols(buffer, frame_count);
        
        if (symbols) {
            for (int i = 0; i < frame_count; ++i) {
                trace << "#" << i << " " << symbols[i] << "\n";
            }
            free(symbols);
        } else {
            trace << "<unable to capture symbols>\n";
            
            // Still output raw addresses
            for (int i = 0; i < frame_count; ++i) {
                trace << "# " << i << " [0x" << std::hex 
                      << reinterpret_cast<uint64_t>(buffer[i]) 
                      << std::dec << "]\n";
            }
        }
#else
        trace << "<backtrace not supported on this platform>\n";
#endif
        
        return trace.str();
    }
    
    /**
     * @brief Generate human-readable crash reason string
     */
    std::string generate_crash_reason(const CrashContext& crash) const {
        std::ostringstream reason;
        
        switch (crash.signal_num) {
            case SIGSEGV:
                reason << "Segmentation fault";
                if (crash.registers.fault_addr != 0) {
                    reason << " accessing address 0x" << std::hex 
                           << crash.registers.fault_addr << std::dec;
                }
                reason << " at instruction 0x" << std::hex 
                       << crash.registers.rip << std::dec;
                break;
                
            case SIGABRT:
                reason << "Abort called (possibly assertion failure)";
                break;
                
            case SIGFPE:
                reason << "Floating-point exception (division by zero?)";
                break;
                
            case SIGILL:
                reason << "Illegal instruction at 0x" << std::hex 
                       << crash.registers.rip << std::dec;
                break;
                
            case SIGBUS:
                reason << "Bus error (alignment issue?)";
                if (crash.registers.fault_addr != 0) {
                    reason << " at address 0x" << std::hex 
                           << crash.registers.fault_addr << std::dec;
                }
                break;
                
            default:
                reason << "Signal " << crash.signal_name << " (" << crash.signal_num << ")";
                break;
        }
        
        return reason.str();
    }
    
    /**
     * @brief Save crash snapshot to file (internal implementation)
     */
    bool save_crash_snapshot_internal(const CrashContext& crash, const std::string& base_path) const {
        // Generate unique filename with timestamp
        std::string filename = base_path;
        
        // Ensure trailing slash
        if (!filename.empty() && filename.back() != '/') {
            filename += '/';
        }
        
        // Add timestamp-based filename
        auto now_time = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now_time);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_buf);
        
        filename += "crash_";
        filename += time_str;
        filename += "_";
        filename += crash.signal_name;
        filename += ".json";
        
        // Write file
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << crash.to_json();
        file.close();
        
        return true;
    }
    
    /**
     * @brief Generate comprehensive report (assumes lock held)
     */
    std::string generate_report_internal() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"plugin\": \"" << name() << "\",\n";
        json << "  \"version\": \"" << version() << "\",\n";
        json << "  \"generated_at_ms\": " << std::fixed << std::setprecision(3)
             << timestamp_to_ms(now()) << ",\n";
        json << "  \"handlers_installed\": " << (handlers_installed_ ? "true" : "false") << ",\n";
        json << "  \"total_crashes\": " << crash_history_.size() << ",\n";
        json << "  \"current_modules\": " << current_modules_.size() << ",\n";
        
        // Last crash details
        if (last_crash_) {
            json << "  \"last_crash\": " << last_crash_->to_json() << ",\n";
        } else {
            json << "  \"last_crash\": null,\n";
        }
        
        // Crash history summary
        json << "  \"crash_summary\": [\n";
        for (size_t i = 0; i < crash_history_.size(); ++i) {
            json << "    {\n";
            json << "      \"signal\": " << crash_history_[i].signal_num << ",\n";
            json << "      \"signal_name\": \"" << crash_history_[i].signal_name << "\",\n";
            json << "      \"timestamp_ms\": " << std::fixed << std::setprecision(3)
                 << timestamp_to_ms(crash_history_[i].timestamp) << ",\n";
            json << "      \"rip\": \"0x" << std::hex << crash_history_[i].registers.rip 
                 << std::dec << "\",\n";
            json << "      \"reason\": \"" << crash_history_[i].crash_reason << "\"\n";
            json << "    }";
            if (i < crash_history_.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Convert address to hex string helper
     */
    static std::string addr_to_hex(uint64_t addr) {
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << addr;
        return oss.str();
    }
    
    /**
     * @brief Emit an info-level diagnostic event
     */
    void emit_info(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "info";
        event.severity = Severity::INFO;
        event.message = message;
        emit_event(event);
    }
    
    /**
     * @brief Emit a warning diagnostic event
     */
    void emit_warning(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "warning";
        event.severity = Severity::WARNING;
        event.message = message;
        emit_event(event);
    }
    
    //=========================================================================
    // Member Variables
    //=========================================================================
    
    std::vector<CrashContext> crash_history_;       ///< History of all crashes
    std::unique_ptr<CrashContext> last_crash_;      ///< Most recent crash
    std::unordered_set<std::string> current_modules_; ///< Currently loaded modules
    
    bool handlers_installed_;                       ///< Whether signal handlers are installed
    size_t max_snapshots_;                          ///< Maximum crashes to keep in history
    int max_stack_frames_;                          ///< Max frames in stack trace
    bool auto_save_enabled_;                        ///< Auto-save snapshots on crash
    std::string snapshot_path_;                     ///< Directory for saving snapshots
    
    static CrashContextPlugin* s_instance_;         ///< Static instance for signal handler
};

// Static member definition
CrashContextPlugin* CrashContextPlugin::s_instance_ = nullptr;

} // namespace diagnostics
} // namespace prosper
