// diagnostics/cli/cli.hpp — CLI flag handling for diagnostics
//
// Parses --diagnostics, --ai-report, --trace-* flags from command line.
// Modifies argc/argv to consume recognized flags.
#pragma once

#include "../core/context.hpp"
#include <string>
#include <vector>

namespace prosper {
namespace diagnostics {

struct ParsedCliFlags {
    DiagnosticsConfig config;
    bool              help_requested = false;
    std::string       error_message;  // Parse error, if any
    
    // Game path (positional argument after flags)
    std::string       game_path;
};

class DiagnosticsCli {
public:
    // Parse command line arguments for diagnostics flags.
    // Returns parsed configuration. Consumes recognized flags from argv.
    // If help is requested or there's an error, sets those fields.
    static ParsedCliFlags parse(int* argc, char*** argv);
    
    // Print usage information for diagnostics flags
    static void print_usage();
    
    // Validate configuration (check output directory writable, etc.)
    static bool validate(const DiagnosticsConfig& config, std::string* error);

private:
    static bool match_flag(const char* arg, const char* short_flag, 
                           const char* long_flag);
    static std::string get_flag_value(const char* arg, const char* next_arg,
                                      int* argc, char*** argv, int i);
};

} // namespace diagnostics
} // namespace prosper
