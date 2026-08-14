/**
 * Debug Intelligence Layer - Main Entry Point
 * 
 * CLI application for evidence management and reasoning assistance
 * in AI-assisted debugging workflows for Prosper/SharpEmuT24.
 * 
 * Usage:
 *   debug-intel <command> [options] [arguments]
 * 
 * Design: Observer-only - no emulator behavior modifications
 */

#include "debug_intelligence.hpp"
#include "cli_interface.hpp"

#include <iostream>
#include <vector>
#include <string>

using namespace debug_intelligence;

/**
 * Print usage information
 */
void printUsage(const char* program_name) {
    std::cerr << "Debug Intelligence Layer v" << VERSION << "\n\n";
    std::cerr << "Usage: " << program_name << " <command> [options] [arguments]\n\n";
    std::cerr << "Run '" << program_name << " --help' for full command reference.\n";
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Need at least a command
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Determine base directory for experiments
    // Default: ./experiments, override with DEBUG_INTEL_DIR env var
    fs::path base_dir = std::getenv("DEBUG_INTEL_DIR") 
        ? fs::path(std::getenv("DEBUG_INTEL_DIR"))
        : fs::current_path() / "experiments";
    
    try {
        // Create CLI interface
        CliInterface cli(base_dir);
        
        // Execute command
        CliResult result = cli.execute(argc, argv);
        
        // Handle result
        if (result.exit_code != 0 && !result.message.empty()) {
            std::cerr << "Error: " << result.message << "\n";
        } else if (!result.message.empty()) {
            // Success message to stdout if needed (usually commands print their own output)
        }
        
        return result.exit_code;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred.\n";
        return 3;
    }
}
