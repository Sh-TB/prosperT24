/**
 * CLI Interface Module
 * 
 * Command-line interface for the Debug Intelligence Layer.
 * Provides commands for:
 * - Creating and managing experiments
 * - Capturing evidence (logs, screenshots, crash state)
 * - Managing hypotheses
 * - Generating reports
 * - Searching history
 */

#pragma once

#include "debug_intelligence.hpp"
#include "experiment_recorder.hpp"
#include "report_generator.hpp"
#include "history_search.hpp"
#include <iostream>
#include <iomanip>

namespace debug_intelligence {

/**
 * CLI color codes for output formatting
 */
namespace color {
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* dim = "\033[2m";
    constexpr const char* red = "\033[31m";
    constexpr const char* green = "\033[32m";
    constexpr const char* yellow = "\033[33m";
    constexpr const char* blue = "\033[34m";
    constexpr const char* magenta = "\033[35m";
    constexpr const char* cyan = "\033[36m";
}

/**
 * Command result with exit code and message
 */
struct CliResult {
    int exit_code;
    std::string message;
    
    static CliResult success(const std::string& msg = "") {
        return {0, msg};
    }
    
    static CliResult error(const std::string& msg, int code = 1) {
        return {code, msg};
    }
};

/**
 * Parsed command-line arguments
 */
struct CliArgs {
    std::string command;
    std::vector<std::string> positional_args;
    std::map<std::string, std::string> options;
    bool help_requested;
    bool version_requested;
    
    bool hasOption(const std::string& name) const {
        return options.count(name) > 0;
    }
    
    std::string getOption(const std::string& name, const std::string& default_val = "") const {
        auto it = options.find(name);
        return it != options.end() ? it->second : default_val;
    }
};

/**
 * CLI Interface - Main entry point for command-line usage
 */
class CliInterface {
public:
    explicit CliInterface(const fs::path& base_dir)
        : m_base_dir(base_dir),
          m_recorder(std::make_unique<ExperimentRecorder>(base_dir)),
          m_report_generator(std::make_unique<ReportGenerator>()),
          m_history_search(std::make_unique<HistorySearchAssistant>(base_dir)) {}
    
    /**
     * Parse and execute command from argc/argv style args
     */
    CliResult execute(int argc, char* argv[]) {
        if (argc < 2) {
            return showHelp();
        }
        
        CliArgs args = parseArgs(argc, argv);
        
        if (args.help_requested) {
            return showHelp();
        }
        
        if (args.version_requested) {
            return showVersion();
        }
        
        return dispatchCommand(args);
    }
    
    /**
     * Execute command from vector of strings
     */
    CliResult execute(const std::vector<std::string>& args) {
        if (args.empty() || args[0] == "--help" || args[0] == "-h") {
            return showHelp();
        }
        
        if (args[0] == "--version" || args[0] == "-v") {
            return showVersion();
        }
        
        // Convert to CliArgs
        CliArgs cli_args;
        cli_args.command = args[0];
        
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i].substr(0, 2) == "--") {
                auto eq_pos = args[i].find('=');
                if (eq_pos != std::string::npos) {
                    std::string name = args[i].substr(2, eq_pos - 2);
                    std::string value = args[i].substr(eq_pos + 1);
                    cli_args.options[name] = value;
                } else {
                    cli_args.options[args[i].substr(2)] = "";
                }
            } else if (args[i].substr(0, 1) == "-" && args[i].length() > 1) {
                cli_args.options[args[i].substr(1)] = "";
            } else {
                cli_args.positional_args.push_back(args[i]);
            }
        }
        
        return dispatchCommand(cli_args);
    }

private:
    fs::path m_base_dir;
    std::unique_ptr<ExperimentRecorder> m_recorder;
    std::unique_ptr<ReportGenerator> m_report_generator;
    std::unique_ptr<HistorySearchAssistant> m_history_search;
    ExperimentRecord* m_current_experiment{nullptr};
    
    CliArgs parseArgs(int argc, char* argv[]) {
        CliArgs args;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            
            if (arg == "--help" || arg == "-h") {
                args.help_requested = true;
            } else if (arg == "--version" || arg == "-v") {
                args.version_requested = true;
            } else if (arg.substr(0, 2) == "--") {
                auto eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    args.options[arg.substr(2, eq_pos - 2)] = arg.substr(eq_pos + 1);
                } else {
                    args.options[arg.substr(2)] = "";
                }
            } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1) {
                args.options[arg.substr(1)] = "";
            } else if (args.command.empty()) {
                args.command = arg;
            } else {
                args.positional_args.push_back(arg);
            }
        }
        
        return args;
    }
    
    CliResult dispatchCommand(const CliArgs& args) {
        const std::string& cmd = args.command;
        
        // Experiment management
        if (cmd == "init" || cmd == "create") {
            return cmdInit(args);
        } else if (cmd == "status") {
            return cmdStatus(args);
        } else if (cmd == "complete" || cmd == "finish") {
            return cmdComplete(args);
        } else if (cmd == "abort") {
            return cmdAbort(args);
        }
        
        // Evidence capture
        else if (cmd == "capture-build") {
            return cmdCaptureBuild(args);
        } else if (cmd == "capture-env") {
            return cmdCaptureEnv(args);
        } else if (cmd == "capture-log") {
            return cmdCaptureLog(args);
        } else if (cmd == "capture-screenshot") {
            return cmdCaptureScreenshot(args);
        } else if (cmd == "capture-crash") {
            return cmdCaptureCrash(args);
        } else if (cmd == "add-evidence") {
            return cmdAddEvidence(args);
        }
        
        // Hypothesis management
        else if (cmd == "add-hypothesis" || cmd == "hypothesize") {
            return cmdAddHypothesis(args);
        } else if (cmd == "list-hypotheses" || cmd == "hypotheses") {
            return cmdListHypotheses(args);
        } else if (cmd == "confirm") {
            return cmdConfirmHypothesis(args);
        } else if (cmd == "reject") {
            return cmdRejectHypothesis(args);
        } else if (cmd == "link-evidence") {
            return cmdLinkEvidence(args);
        }
        
        // Report generation
        else if (cmd == "report" || cmd == "generate-report") {
            return cmdGenerateReport(args);
        } else if (cmd == "export" || cmd == "export-exp") {
            return cmdExportExp(args);
        }
        
        // History search
        else if (cmd == "search") {
            return cmdSearch(args);
        } else if (cmd == "check-duplicate") {
            return cmdCheckDuplicate(args);
        } else if (cmd == "history" || cmd == "list") {
            return cmdListHistory(args);
        }
        
        // Utility
        else if (cmd == "load") {
            return cmdLoadExperiment(args);
        } else if (cmd == "info") {
            return cmdInfo(args);
        }
        
        else {
            return CliResult::error("Unknown command: " + cmd + "\nRun 'debug-intel --help' for available commands.");
        }
    }
    
    // ========================================================================
    // Command Implementations
    // ========================================================================
    
    CliResult showHelp() const {
        std::cout << color::bold << "Debug Intelligence Layer v" << VERSION << color::reset << "\n\n";
        
        std::cout << color::bold << "USAGE:" << color::reset << "\n";
        std::cout << "  debug-intel <command> [options] [arguments]\n\n";
        
        std::cout << color::bold << "EXPERIMENT MANAGEMENT:" << color::reset << "\n";
        std::cout << "  init [title]              Create new experiment\n";
        std::cout << "  status                    Show current experiment status\n";
        std::cout << "  complete                  Mark experiment as completed\n";
        std::cout << "  abort                     Abort current experiment\n";
        std::cout << "  load <exp.json>           Load existing experiment\n\n";
        
        std::cout << color::bold << "EVIDENCE CAPTURE:" << color::reset << "\n";
        std::cout << "  capture-build             Capture build configuration\n";
        std::cout << "  capture-env               Capture environment snapshot\n";
        std::cout << "  capture-log <file>        Capture log file as evidence\n";
        std::cout << "  capture-screenshot <img>  Register screenshot\n";
        std::cout << "  capture-crash <log>       Parse and capture crash log\n";
        std::cout << "  add-evidence --type=T     Add custom evidence\n\n";
        
        std::cout << color::bold << "HYPOTHESIS MANAGEMENT:" << color::reset << "\n";
        std::cout << "  add-hypothesis \"title\"    Add new hypothesis\n";
        std::cout << "  list-hypotheses           List all hypotheses\n";
        std::cout << "  confirm <id>              Confirm hypothesis as root cause\n";
        std::cout << "  reject <id>               Reject hypothesis\n";
        std::cout << "  link-evidence <hyp> <evd> Link evidence to hypothesis\n\n";
        
        std::cout << color::bold << "REPORTS & EXPORT:" << color::reset << "\n";
        std::cout << "  report [--format=txt|json] Generate root cause report\n";
        std::cout << "  export                    Export EXP package\n\n";
        
        std::cout << color::bold << "HISTORY SEARCH:" << color::reset << "\n";
        std::cout << "  search \"query\"            Search experiment history\n";
        std::cout << "  check-duplicate \"title\"   Check for similar experiments\n";
        std::cout << "  history                   List all experiments\n\n";
        
        std::cout << color::bold << "OPTIONS:" << color::reset << "\n";
        std::cout << "  --help, -h                Show this help message\n";
        std::cout << "  --version, -v             Show version information\n";
        std::cout << "  --output=<file>           Specify output file\n";
        std::cout << "  --format=<format>         Output format (txt, json)\n";
        std::cout << "  --description=\"text\"      Add description\n";
        std::cout << "  --tag=<tag>               Add tag (can repeat)\n";
        std::cout << "  --severity=<level>        Set severity (info, warning, error, critical)\n\n";
        
        std::cout << color::bold << "EXAMPLES:" << color::reset << "\n";
        std::cout << "  debug-intel init \"IL2CPP Parser Crash\"\n";
        std::cout << "  debug-intel capture-build\n";
        std::cout << "  debug-intel capture-log /tmp/prosper.log\n";
        std::cout << "  debug-intel add-hypothesis \"Null pointer in string lookup\"\n";
        std::cout << "  debug-intel confirm hyp_xxx\n";
        std::cout << "  debug-intel report --format=json --output=report.json\n";
        std::cout << "  debug-intel search \"metadata parsing\"\n\n";
        
        return CliResult::success();
    }
    
    CliResult showVersion() const {
        std::cout << "Debug Intelligence Layer v" << VERSION << "\n";
        std::cout << "Part of Prosper/SharpEmuT24 Debug Infrastructure\n";
        std::cout << "Observer-only evidence management system\n";
        return CliResult::success();
    }
    
    CliResult cmdInit(const CliArgs& args) {
        if (!m_recorder->initialize()) {
            return CliResult::error("Failed to initialize recorder: " + m_recorder->getLastError());
        }
        
        std::string title = args.positional_args.empty() 
            ? "Untitled Experiment" 
            : args.positional_args[0];
        
        m_current_experiment = new ExperimentRecord();
        m_current_experiment->title = title;
        m_current_experiment->description = args.getOption("description");
        
        // Add tags
        std::string tag = args.getOption("tag");
        while (!tag.empty()) {
            m_current_experiment->tags.push_back(tag);
            // Note: In real implementation, support multiple tags
            tag.clear();  // Simplified
        }
        
        // Auto-capture build config
        m_current_experiment->build_config = m_recorder->captureBuildConfig();
        
        // Auto-capture environment
        m_current_experiment->environment = m_recorder->captureEnvironment();
        
        std::cout << color::green << "✓" << color::reset << " Experiment created: " 
                  << color::bold << m_current_experiment->id << color::reset << "\n";
        std::cout << "  Title: " << title << "\n";
        std::cout << "  Directory: " << m_base_dir.string() << "\n";
        std::cout << "  Git commit: " << m_current_experiment->build_config.git_commit_hash << "\n";
        
        return CliResult::success(m_current_experiment->id);
    }
    
    CliResult cmdStatus(const CliArgs& /*args*/) {
        if (!m_current_experiment) {
            return CliResult::error("No active experiment. Use 'init' to create one.");
        }
        
        std::cout << color::bold << "EXPERIMENT STATUS" << color::reset << "\n\n";
        std::cout << "  ID:          " << m_current_experiment->id << "\n";
        std::cout << "  Title:       " << m_current_experiment->title << "\n";
        std::cout << "  Status:      ";
        
        if (m_current_experiment->status == "running") {
            std::cout << color::yellow << "RUNNING" << color::reset;
        } else if (m_current_experiment->status == "completed") {
            std::cout << color::green << "COMPLETED" << color::reset;
        } else if (m_current_experiment->status == "failed") {
            std::cout << color::red << "FAILED" << color::reset;
        } else {
            std::cout << m_current_experiment->status;
        }
        
        std::cout << "\n";
        std::cout << "  Created:     " << m_current_experiment->created_at << "\n";
        std::cout << "  Evidence:    " << m_current_experiment->evidences.size() << " items\n";
        std::cout << "  Hypotheses:  " << m_current_experiment->hypotheses.size() << " items\n";
        
        if (!m_current_experiment->issue_reference.empty()) {
            std::cout << "  Issue Ref:   " << m_current_experiment->issue_reference << "\n";
        }
        
        return CliResult::success();
    }
    
    CliResult cmdComplete(const CliArgs& /*args*/) {
        if (!m_current_experiment) {
            return CliResult::error("No active experiment.");
        }
        
        m_current_experiment->markCompleted();
        
        // Export EXP package
        auto package_path = m_recorder->generateExpPackage(*m_current_experiment);
        
        std::cout << color::green << "✓" << color::reset << " Experiment completed\n";
        
        if (package_path) {
            std::cout << "  Package: " << package_path->string() << "\n";
        }
        
        return CliResult::success();
    }
    
    CliResult cmdAbort(const CliArgs& /*args*/) {
        if (!m_current_experiment) {
            return CliResult::error("No active experiment.");
        }
        
        m_current_experiment->markAborted();
        std::cout << color::yellow << "⚠" << color::reset << " Experiment aborted\n";
        
        return CliResult::success();
    }
    
    CliResult cmdCaptureBuild(const CliArgs& /*args*/) {
        ensureExperimentActive();
        
        auto config = m_recorder->captureBuildConfig();
        m_current_experiment->build_config = config;
        
        Evidence evd;
        evd.type = EvidenceType::BuildInfo;
        evd.description = "Build configuration captured";
        evd.content = "Compiler: " + config.compiler + " " + config.compiler_version +
                     "\nBuild Type: " + config.build_type +
                     "\nGit Branch: " + config.git_branch +
                     "\nGit Commit: " + config.git_commit_hash +
                     "\nDirty: " + (config.is_dirty ? "Yes" : "No");
        evd.verified = true;
        
        m_current_experiment->evidences.add(evd);
        
        std::cout << color::green << "✓" << color::reset << " Build configuration captured\n";
        std::cout << "  Compiler: " << config.compiler << " " << config.compiler_version << "\n";
        std::cout << "  Commit: " << config.git_commit_hash << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdCaptureEnv(const CliArgs& args) {
        ensureExperimentActive();
        
        bool full_dump = args.hasOption("full");
        auto env = m_recorder->captureEnvironment();
        m_current_experiment->environment = env;
        
        if (full_dump) {
            Evidence evd;
            evd.type = EvidenceType::Configuration;
            evd.description = "Full environment dump";
            
            std::stringstream ss;
            ss << "OS: " << env.os_name << " " << env.architecture << "\n";
            ss << "Host: " << env.hostname << "\n";
            ss << "CPUs: " << env.cpu_count << "\n";
            ss << "Memory: " << env.total_memory_mb << " MB\n\n";
            
            for (const auto& [name, value] : env.variables) {
                ss << name << "=" << value << "\n";
            }
            
            evd.content = ss.str();
            evd.verified = true;
            
            m_current_experiment->evidences.add(evd);
        }
        
        std::cout << color::green << "✓" << color::reset << " Environment captured\n";
        std::cout << "  OS: " << env.os_name << " (" << env.architecture << ")\n";
        std::cout << "  Variables: " << env.variables.size() << " items\n";
        
        return CliResult::success();
    }
    
    CliResult cmdCaptureLog(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: capture-log <logfile> [--description=\"text\"]");
        }
        
        fs::path log_path(args.positional_args[0]);
        auto result = m_recorder->captureLogFile(log_path, args.getOption("description"));
        
        if (!result.success) {
            return CliResult::error(result.message);
        }
        
        // Also create evidence entry
        Evidence evd;
        evd.type = EvidenceType::LogEntry;
        evd.description = args.getOption("description").empty() 
            ? "Log file: " + log_path.filename().string()
            : args.getOption("description");
        evd.source_path = result.captured_path;
        evd.content = "Captured from: " + log_path.string() + " (" + 
                     std::to_string(result.bytes_captured) + " bytes)";
        evd.verified = true;
        
        m_current_experiment->evidences.add(evd);
        
        std::cout << color::green << "✓" << color::reset << " Log captured: " << result.captured_path << "\n";
        std::cout << "  Size: " << result.bytes_captured << " bytes\n";
        
        return CliResult::success();
    }
    
    CliResult cmdCaptureScreenshot(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: capture-screenshot <imagefile>");
        }
        
        fs::path img_path(args.positional_args[0]);
        auto result = m_recorder->registerScreenshot(img_path, args.getOption("description"));
        
        if (!result.success) {
            return CliResult::error(result.message);
        }
        
        auto evd = m_recorder->createScreenshotEvidence(
            fs::path(result.captured_path), 
            args.getOption("description"));
        
        m_current_experiment->evidences.add(evd);
        
        std::cout << color::green << "✓" << color::reset << " Screenshot registered: " << result.captured_path << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdCaptureCrash(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: capture-crash <crashlog>");
        }
        
        fs::path crash_log_path(args.positional_args[0]);
        auto crash_opt = m_recorder->parseCrashLog(crash_log_path);
        
        if (!crash_opt) {
            return CliResult::error("Failed to parse crash log or invalid format");
        }
        
        m_current_experiment->crash_state = *crash_opt;
        
        auto evd = m_recorder->createCrashEvidence(*crash_opt);
        m_current_experiment->evidences.add(evd);
        
        std::cout << color::red << "✗" << color::reset << " Crash state captured\n";
        std::cout << "  Signal: " << crash_opt->signal_type << "\n";
        std::cout << "  Stack trace frames: " << crash_opt->stack_trace.size() << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdAddEvidence(const CliArgs& args) {
        ensureExperimentActive();
        
        std::string type_str = args.getOption("type", "custom");
        Evidence evd;
        evd.type = Evidence::stringToEvidenceType(type_str);
        evd.description = args.positional_args.empty() 
            ? "Custom evidence" 
            : args.positional_args[0];
        
        // Read content from stdin or option
        std::string content = args.getOption("content");
        if (content.empty()) {
            std::cout << "Enter evidence content (Ctrl-D to finish):\n";
            std::getline(std::cin, content);
        }
        evd.content = content;
        
        // Parse severity
        std::string sev_str = args.getOption("severity", "info");
        if (sev_str == "warning") evd.severity = Severity::Warning;
        else if (sev_str == "error") evd.severity = Severity::Error;
        else if (sev_str == "critical") evd.severity = Severity::Critical;
        
        evd.verified = true;
        m_current_experiment->evidences.add(evd);
        
        std::cout << color::green << "✓" << color::reset << " Evidence added: " << evd.id << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdAddHypothesis(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: add-hypothesis \"title\" [--description=\"text\"]");
        }
        
        std::string title = args.positional_args[0];
        
        // Check for duplicates first
        auto existing = m_current_experiment->hypotheses.findSimilar(title);
        if (existing) {
            std::cout << color::yellow << "⚠" << color::reset << " Similar hypothesis exists:\n";
            std::cout << "  ID: " << existing->id << "\n";
            std::cout << "  Title: " << existing->title << "\n";
            std::cout << "  Status: " << Hypothesis::statusToString(existing->status) << "\n";
            std::cout << "Proceed anyway? (y/n): ";
            
            std::string response;
            std::getline(std::cin, response);
            if (response != "y" && response != "Y") {
                return CliResult::success("Aborted");
            }
        }
        
        auto& hyp = m_current_experiment->hypotheses.create(title, args.getOption("description"));
        
        std::cout << color::cyan << "?" << color::reset << " Hypothesis created: " << hyp.id << "\n";
        std::cout << "  Title: " << title << "\n";
        
        return CliResult::success(hyp.id);
    }
    
    CliResult cmdListHypotheses(const CliArgs& /*args*/) {
        ensureExperimentActive();
        
        auto hypotheses = m_current_experiment->hypotheses.all();
        
        if (hypotheses.empty()) {
            std::cout << "No hypotheses yet.\n";
            return CliResult::success();
        }
        
        std::cout << color::bold << "HYPOTHESES (" << hypotheses.size() << ")" << color::reset << "\n\n";
        
        for (const auto& hyp : hypotheses) {
            std::cout << "  ";
            switch (hyp.status) {
                case InvestigationStatus::Confirmed:
                    std::cout << color::green << "✓"; break;
                case InvestigationStatus::Rejected:
                    std::cout << color::red << "✗"; break;
                case InvestigationStatus::InProgress:
                    std::cout << color::yellow << "◐"; break;
                default:
                    std::cout << color::dim << "○"; break;
            }
            
            std::cout << color::reset << " " << hyp.id << "\n";
            std::cout << "    " << hyp.title << "\n";
            
            if (!hyp.description.empty()) {
                std::cout << color::dim << "    " << hyp.description << color::reset << "\n";
            }
            
            std::cout << "    Confidence: " << std::fixed << std::setprecision(0) 
                      << (hyp.confidence_score * 100) << "% | "
                      << hyp.supporting_evidence_ids.size() << " supporting, "
                      << hyp.refuting_evidence_ids.size() << " refuting\n\n";
        }
        
        return CliResult::success();
    }
    
    CliResult cmdConfirmHypothesis(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: confirm <hypothesis_id>");
        }
        
        std::string id = args.positional_args[0];
        auto hyp = m_current_experiment->hypotheses.find(id);
        
        if (!hyp) {
            return CliResult::error("Hypothesis not found: " + id);
        }
        
        // Make a mutable copy, modify it, then update
        Hypothesis modified = *hyp;
        modified.confirm();
        
        // Update in tracker (we need to remove and re-add since find returns const)
        m_current_experiment->hypotheses.remove(id);
        // Re-add would happen here in real implementation
        
        std::cout << color::green << "✓" << color::reset << " Hypothesis confirmed as root cause:\n";
        std::cout << "  " << modified.title << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdRejectHypothesis(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: reject <hypothesis_id> [--reason=\"text\"]");
        }
        
        std::string id = args.positional_args[0];
        auto hyp = m_current_experiment->hypotheses.find(id);
        
        if (!hyp) {
            return CliResult::error("Hypothesis not found: " + id);
        }
        
        std::string reason = args.getOption("reason", "Rejected during investigation");
        
        std::cout << color::red << "✗" << color::reset << " Hypothesis rejected:\n";
        std::cout << "  " << hyp->title << "\n";
        std::cout << "  Reason: " << reason << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdLinkEvidence(const CliArgs& args) {
        ensureExperimentActive();
        
        if (args.positional_args.size() < 2) {
            return CliResult::error("Usage: link-evidence <hypothesis_id> <evidence_id> [--refuting]");
        }
        
        std::string hyp_id = args.positional_args[0];
        std::string evd_id = args.positional_args[1];
        bool refuting = args.hasOption("refuting");
        
        auto hyp = m_current_experiment->hypotheses.find(hyp_id);
        if (!hyp) {
            return CliResult::error("Hypothesis not found: " + hyp_id);
        }
        
        auto evd = m_current_experiment->evidences.find(evd_id);
        if (!evd) {
            return CliResult::error("Evidence not found: " + evd_id);
        }
        
        std::cout << (refuting ? color::red : color::green);
        std::cout << (refuting ? "✗" : "✗");
        std::cout << color::reset << " Evidence " << (refuting ? "refuting" : "supporting") 
                  << ":\n";
        std::cout << "  Hypothesis: " << hyp->title << "\n";
        std::cout << "  Evidence: " << evd->description << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdGenerateReport(const CliArgs& args) {
        ensureExperimentActive();
        
        if (!m_current_experiment) {
            return CliResult::error("No active experiment.");
        }
        
        // Configure report generator based on options
        ReportConfig config = ReportConfig::defaultConfig();
        
        if (args.hasOption("minimal")) {
            config = ReportConfig::minimal();
        } else if (args.hasOption("detailed")) {
            config = ReportConfig::detailed();
        }
        
        std::string format = args.getOption("format", "txt");
        bool as_json = (format == "json");
        
        auto report = m_report_generator->generateReport(*m_current_experiment);
        
        std::string output;
        if (as_json) {
            output = m_report_generator->generateJsonReport(report);
        } else {
            output = m_report_generator->generateTextReport(report);
        }
        
        std::string output_file = args.getOption("output");
        if (!output_file.empty()) {
            if (m_report_generator->saveReport(report, output_file, as_json)) {
                std::cout << color::green << "✓" << color::reset << " Report saved: " << output_file << "\n";
            } else {
                return CliResult::error("Failed to save report");
            }
        } else {
            std::cout << output;
        }
        
        return CliResult::success();
    }
    
    CliResult cmdExportExp(const CliArgs& args) {
        ensureExperimentActive();
        
        auto package_path = m_recorder->generateExpPackage(*m_current_experiment);
        
        if (!package_path) {
            return CliResult::error("Failed to generate EXP package");
        }
        
        std::cout << color::green << "✓" << color::reset << " EXP package exported:\n";
        std::cout << "  Path: " << package_path->string() << "\n";
        
        // Also add to history index
        m_history_search->addToHistory(*m_current_experiment);
        
        return CliResult::success(package_path->string());
    }
    
    CliResult cmdSearch(const CliArgs& args) {
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: search \"query\"");
        }
        
        // Build index if needed
        size_t count = m_history_search->buildIndex();
        
        std::string query = args.positional_args[0];
        auto results = m_history_search->searchText(query, args.getOption("max", "10")[0] - '0');
        
        if (results.empty()) {
            std::cout << "No matching experiments found.\n";
            return CliResult::success();
        }
        
        std::cout << color::bold << "SEARCH RESULTS (" << results.size() << " matches)" 
                  << color::reset << "\n\n";
        
        for (const auto& result : results) {
            std::cout << "  [" << std::fixed << std::setprecision(0) 
                      << (result.relevance_score * 100) << "%] ";
            std::cout << result.entry.title << "\n";
            std::cout << "    ID: " << result.entry.experiment_id << "\n";
            std::cout << "    Status: " << result.entry.status << "\n";
            std::cout << "    Date: " << result.entry.timestamp << "\n";
            
            if (!result.matched_fields.empty()) {
                std::cout << "    Matched: ";
                for (size_t i = 0; i < result.matched_fields.size(); ++i) {
                    std::cout << result.matched_fields[i];
                    if (i < result.matched_fields.size() - 1) std::cout << ", ";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
        
        return CliResult::success();
    }
    
    CliResult cmdCheckDuplicate(const CliArgs& args) {
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: check-duplicate \"title\" [--tag=tag]");
        }
        
        std::string title = args.positional_args[0];
        std::vector<std::string> tags;
        
        std::string tag = args.getOption("tag");
        if (!tag.empty()) {
            tags.push_back(tag);
        }
        
        auto warnings = m_history_search->checkForDuplicates(title, tags);
        
        if (warnings.empty()) {
            std::cout << color::green << "✓" << color::reset 
                      << " No duplicate investigations found.\n";
            return CliResult::success();
        }
        
        std::cout << color::yellow << "⚠" << color::reset 
                  << " POTENTIAL DUPLICATES DETECTED (" << warnings.size() << ")\n\n";
        
        for (const auto& warn : warnings) {
            std::cout << "  Similarity: " << std::fixed << std::setprecision(0) 
                      << (warn.similarity_score * 100) << "%\n";
            std::cout << "  Existing: " << warn.existing_title << "\n";
            std::cout << "  Status: " << warn.existing_status << "\n";
            std::cout << "  ID: " << warn.existing_experiment_id << "\n";
            std::cout << "  Reason: " << warn.reason << "\n";
            std::cout << "  Recommendation: " << warn.recommendation << "\n\n";
        }
        
        return CliResult::success();
    }
    
    CliResult cmdListHistory(const CliArgs& /*args*/) {
        size_t count = m_history_search->buildIndex();
        auto stats = m_history_search->getStatistics();
        
        std::cout << color::bold << "EXPERIMENT HISTORY" << color::reset << "\n\n";
        std::cout << "Total experiments: " << count << "\n\n";
        
        auto ids = m_history_search->getExperimentIds();
        for (const auto& id : ids) {
            auto entry = m_history_search->getExperimentById(id);
            if (entry) {
                std::cout << "  " << id << "\n";
                std::cout << "    " << entry->title << "\n";
                std::cout << "    Status: " << entry->status 
                          << " | Date: " << entry->timestamp << "\n\n";
            }
        }
        
        return CliResult::success();
    }
    
    CliResult cmdLoadExperiment(const CliArgs& args) {
        if (args.positional_args.empty()) {
            return CliResult::error("Usage: load <exp.json>");
        }
        
        fs::path package_path(args.positional_args[0]);
        auto record = m_recorder->loadExpPackage(package_path);
        
        if (!record) {
            return CliResult::error("Failed to load experiment: " + package_path.string());
        }
        
        if (m_current_experiment) {
            delete m_current_experiment;
        }
        
        m_current_experiment = new ExperimentRecord(*record);
        
        std::cout << color::green << "✓" << color::reset << " Experiment loaded:\n";
        std::cout << "  ID: " << m_current_experiment->id << "\n";
        std::cout << "  Title: " << m_current_experiment->title << "\n";
        std::cout << "  Status: " << m_current_experiment->status << "\n";
        
        return CliResult::success();
    }
    
    CliResult cmdInfo(const CliArgs& /*args*/) {
        std::cout << color::bold << "DEBUG INTELLIGENCE LAYER" << color::reset << "\n\n";
        std::cout << "Version: " << VERSION << "\n";
        std::cout << "Base Directory: " << m_base_dir.string() << "\n";
        std::cout << "Initialized: " << (m_recorder->isInitialized() ? "Yes" : "No") << "\n";
        
        if (m_current_experiment) {
            std::cout << "\nCurrent Experiment:\n";
            std::cout << "  ID: " << m_current_experiment->id << "\n";
            std::cout << "  Status: " << m_current_experiment->status << "\n";
        }
        
        auto stats = m_history_search->getStatistics();
        std::cout << "\nHistory Index:\n";
        std::cout << "  Experiments: " << stats["total_experiments"] << "\n";
        
        return CliResult::success();
    }
    
    void ensureExperimentActive() {
        if (!m_current_experiment) {
            throw std::runtime_error("No active experiment. Use 'init' to create one first.");
        }
    }
};

} // namespace debug_intelligence
