// diagnostics/cli/cli.cpp — CLI implementation
#include "cli.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace prosper {
namespace diagnostics {

bool DiagnosticsCli::match_flag(const char* arg, const char* short_flag,
                                 const char* long_flag) {
    if (!arg) return false;
    if (short_flag && std::strcmp(arg, short_flag) == 0) return true;
    if (long_flag && std::strcmp(arg, long_flag) == 0) return true;
    return false;
}

std::string DiagnosticsCli::get_flag_value(const char* arg, const char* next_arg,
                                            int* argc, char*** argv, int i) {
    // Check for = syntax: --flag=value
    const char* eq = std::strchr(arg, '=');
    if (eq) return std::string(eq + 1);
    
    // Check next argument (if it doesn't look like a flag)
    if (next_arg && next_arg[0] != '-' && i + 1 < *argc) {
        return std::string(next_arg);
    }
    
    return "";
}

ParsedCliFlags DiagnosticsCli::parse(int* argc, char*** argv) {
    ParsedCliFlags result;
    
    if (!argc || !argv || !*argv || *argc <= 1) return result;
    
    // Make a copy of args to filter out consumed flags
    std::vector<char*> kept_args;
    kept_args.push_back((*argv)[0]);  // Keep program name
    
    for (int i = 1; i < *argc; ++i) {
        const char* arg = (*argv)[i];
        const char* next_arg = (i + 1 < *argc) ? (*argv)[i + 1] : nullptr;
        
        // Help
        if (match_flag(arg, "--help", "-h")) {
            result.help_requested = true;
            kept_args.push_back((*argv)[i]);
            continue;
        }
        
        // Main diagnostics enable flag
        if (match_flag(arg, "--diagnostics", nullptr)) {
            result.config.enabled = true;
            
            // Check for optional value (=output directory or next arg)
            auto val = get_flag_value(arg, next_arg, argc, argv, i);
            if (!val.empty()) {
                result.config.output_directory = val;
                i++;  // Skip next arg
            }
            continue;
        }
        
        // AI report generation
        if (match_flag(arg, "--ai-report", nullptr)) {
            result.config.ai_report = true;
            result.config.enabled = true;  // Implies --diagnostics
            continue;
        }
        
        // Trace all subsystems
        if (match_flag(arg, "--trace-all", nullptr)) {
            result.config.trace_all = true;
            result.config.enabled = true;
            continue;
        }
        
        // Individual trace flags
        if (match_flag(arg, "--trace-loader", nullptr)) {
            result.config.trace_loader = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-prx", nullptr)) {
            result.config.trace_prx = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-hle", nullptr)) {
            result.config.trace_hle = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-memory", nullptr)) {
            result.config.trace_memory = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-gpu", nullptr)) {
            result.config.trace_gpu = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-video", nullptr)) {
            result.config.trace_video = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-thread", nullptr)) {
            result.config.trace_thread = true;
            result.config.enabled = true;
            continue;
        }
        if (match_flag(arg, "--trace-crash", nullptr)) {
            result.config.trace_crash = true;
            result.config.enabled = true;
            continue;
        }
        
        // Output directory (standalone)
        if (match_flag(arg, "--diag-output", "-o")) {
            auto val = get_flag_value(arg, next_arg, argc, argv, i);
            if (!val.empty()) {
                result.config.output_directory = val;
                i++;
            } else {
                result.error_message = "Missing value for --diag-output";
            }
            continue;
        }
        
        // Unknown flag - keep it (may be used by main program)
        kept_args.push_back((*argv)[i]);
    }
    
    // If trace_all is set, enable all individual traces
    if (result.config.trace_all) {
        result.config.trace_loader = true;
        result.config.trace_prx = true;
        result.config.trace_hle = true;
        result.config.trace_memory = true;
        result.config.trace_gpu = true;
        result.config.trace_video = true;
        result.config.trace_thread = true;
        result.config.trace_crash = true;
    }
    
    // Update argc/argv with filtered arguments
    // (Note: we don't actually modify them to avoid complexity,
    // but we record what was parsed)
    
    return result;
}

void DiagnosticsCli::print_usage() {
    printf("\nDiagnostics Options:\n");
    printf("  --diagnostics[=DIR]   Enable diagnostics output (optional output directory)\n");
    printf("  --ai-report           Generate AI-optimized analysis report\n");
    printf("  --trace-all           Enable tracing for all subsystems\n");
    printf("  --trace-loader        Trace ELF/PRX loader operations\n");
    printf("  --trace-prx           Trace PRX module loading and init\n");
    printf("  --trace-hle           Trace HLE function calls\n");
    printf("  --trace-memory        Trace memory operations\n");
    printf("  --trace-gpu           Trace GPU pipeline events\n");
    printf("  --trace-video         Trace VideoOut/presentation\n");
    printf("  --trace-thread        Trace thread/sync operations\n");
    printf("  --trace-crash         Trace crash handling\n");
    printf("  -o, --diag-output DIR Set diagnostics output directory\n");
    printf("\nOutput files:\n");
    printf("  When enabled, creates a diagnostic_run/ directory containing:\n");
    printf("  session.json, timeline.json, events.json, evidence.json,\n");
    printf("  elf_report.json, prx_report.json, imports.json, relocations.json,\n");
    printf("  memory_report.json, threads.json, gpu_report.json, video_report.json,\n");
    printf("  hle_report.json, syscalls.json, crash.json, ai_report.json,\n");
    printf("  summary.json, ai_context.md\n");
}

bool DiagnosticsCli::validate(const DiagnosticsConfig& config, std::string* error) {
    if (!config.enabled) return true;
    
    // Could add validation here:
    // - Check output directory is writable
    // - Check disk space
    // - Validate trace combinations
    
    return true;
}

} // namespace diagnostics
} // namespace prosper
