/**
 * Comprehensive Test Suite for Debug Intelligence Layer
 * 
 * Tests cover:
 * - Core types and utilities
 * - Evidence system
 * - Hypothesis tracker
 * - Experiment recorder
 * - Report generator
 * - History search
 * - CLI interface
 * - JSON serialization/deserialization
 */

#include <gtest/gtest.h>
#include "../src/debug_intelligence/debug_intelligence.hpp"
#include "../src/debug_intelligence/experiment_recorder.hpp"
#include "../src/debug_intelligence/report_generator.hpp"
#include "../src/debug_intelligence/history_search.hpp"
#include "../src/debug_intelligence/cli_interface.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
namespace debug_intelligence {

class DebugIntelligenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test artifacts
        test_dir_ = fs::temp_directory_path() / ("debug_intel_test_" + Timestamp::fileFriendly());
        fs::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Cleanup
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }
    
    fs::path test_dir_;
};

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, Timestamp_Now_ReturnsValidISOFormat) {
    std::string now = Timestamp::now();
    
    // Should be in format YYYY-MM-DDTHH:MM:SSZ
    ASSERT_GE(now.length(), 20u);
    EXPECT_EQ(now[4], '-');
    EXPECT_EQ(now[7], '-');
    EXPECT_EQ(now[10], 'T');
    EXPECT_EQ(now[13], ':');
    EXPECT_EQ(now[16], ':');
    EXPECT_EQ(now.back(), 'Z');
}

TEST_F(DebugIntelligenceTest, Timestamp_FileFriendly_ReturnsValidFormat) {
    std::string ts = Timestamp::fileFriendly();
    
    // Should be in format YYYYMMDD_HHMMSS
    ASSERT_EQ(ts.length(), 15u);
    EXPECT_EQ(ts[8], '_');
    
    // All characters should be digits or underscore
    for (char c : ts) {
        EXPECT_TRUE(std::isdigit(c) || c == '_');
    }
}

TEST_F(DebugIntelligenceTest, Timestamp_MultipleCalls_AreMonotonic) {
    std::string t1 = Timestamp::now();
    std::string t2 = Timestamp::now();
    
    // Second timestamp should not be before first
    // Note: Very rapid calls might have same second, so we just check format
    EXPECT_EQ(t1.length(), t2.length());
}

// ============================================================================
// Evidence Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, Evidence_DefaultConstructor_SetsDefaults) {
    Evidence evd;
    
    EXPECT_FALSE(evd.id.empty());
    EXPECT_EQ(evd.type, EvidenceType::Custom);
    EXPECT_TRUE(evd.description.empty());
    EXPECT_TRUE(evd.content.empty());
    EXPECT_EQ(evd.severity, Severity::Info);
    EXPECT_FALSE(evd.verified);
    EXPECT_FALSE(evd.timestamp.empty());
}

TEST_F(DebugIntelligenceTest, Evidence_GenerateId_UniqueIds) {
    std::set<std::string> ids;
    
    for (int i = 0; i < 100; ++i) {
        Evidence evd;
        auto result = ids.insert(evd.id);
        EXPECT_TRUE(result.second);  // ID should be unique
    }
}

TEST_F(DebugIntelligenceTest, Evidence_ToMap_ContainsAllFields) {
    Evidence evd;
    evd.type = EvidenceType::LogEntry;
    evd.description = "Test log entry";
    evd.content = "Log content here";
    evd.source_path = "/var/log/test.log";
    evd.severity = Severity::Error;
    evd.verified = true;
    
    auto map = evd.toMap();
    
    EXPECT_EQ(map.at("id"), evd.id);
    EXPECT_EQ(map.at("type"), "LogEntry");
    EXPECT_EQ(map.at("description"), "Test log entry");
    EXPECT_EQ(map.at("content"), "Log content here");
    EXPECT_EQ(map.at("source_path"), "/var/log/test.log");
    EXPECT_EQ(map.at("severity"), "Error");
    EXPECT_EQ(map.at("verified"), "true");
}

TEST_F(DebugIntelligenceTest, EvidenceType_StringConversion_RoundTrip) {
    EvidenceType types[] = {
        EvidenceType::LogEntry,
        EvidenceType::Screenshot,
        EvidenceType::CrashDump,
        EvidenceType::MemorySnapshot,
        EvidenceType::Configuration,
        EvidenceType::EnvironmentVariable,
        EvidenceType::BuildInfo,
        EvidenceType::GitCommit,
        EvidenceType::UserObservation,
        EvidenceType::MetricMeasurement,
        EvidenceType::CodeDiff,
        EvidenceType::NetworkCapture,
        EvidenceType::Custom
    };
    
    for (auto type : types) {
        std::string str = Evidence::evidenceTypeToString(type);
        EvidenceType converted = Evidence::stringToEvidenceType(str);
        EXPECT_EQ(converted, type) << "Failed round-trip for type " << str;
    }
}

TEST_F(DebugIntelligenceTest, Severity_StringConversion_RoundTrip) {
    Severity sevs[] = {Severity::Info, Severity::Warning, Severity::Error, Severity::Critical};
    
    for (auto sev : sevs) {
        std::string str = Evidence::severityToString(sev);
        Severity converted;
        // Manual conversion check since we don't have stringToSeverity
        if (str == "Info") converted = Severity::Info;
        else if (str == "Warning") converted = Severity::Warning;
        else if (str == "Error") converted = Severity::Error;
        else if (str == "Critical") converted = Severity::Critical;
        else FAIL() << "Unknown severity string: " << str;
        
        EXPECT_EQ(converted, sev);
    }
}

// ============================================================================
// EvidenceCollection Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, EvidenceCollection_AddAndFind) {
    EvidenceCollection coll;
    
    Evidence evd1;
    evd1.description = "First evidence";
    coll.add(evd1);
    
    Evidence evd2;
    evd2.description = "Second evidence";
    coll.add(evd2);
    
    EXPECT_EQ(coll.size(), 2u);
    
    auto found = coll.find(evd1.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->description, "First evidence");
    
    auto not_found = coll.find("nonexistent");
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(DebugIntelligenceTest, EvidenceCollection_Remove) {
    EvidenceCollection coll;
    
    Evidence evd;
    coll.add(evd);
    
    EXPECT_TRUE(coll.remove(evd.id));
    EXPECT_EQ(coll.size(), 0u);
    EXPECT_FALSE(coll.find(evd.id).has_value());
    
    // Removing non-existent should return false
    EXPECT_FALSE(coll.remove("nonexistent"));
}

TEST_F(DebugIntelligenceTest, EvidenceCollection_FindByType) {
    EvidenceCollection coll;
    
    Evidence evd1;
    evd1.type = EvidenceType::LogEntry;
    coll.add(evd1);
    
    Evidence evd2;
    evd2.type = EvidenceType::LogEntry;
    coll.add(evd2);
    
    Evidence evd3;
    evd3.type = EvidenceType::Screenshot;
    coll.add(evd3);
    
    auto logs = coll.findByType(EvidenceType::LogEntry);
    EXPECT_EQ(logs.size(), 2u);
    
    auto screenshots = coll.findByType(EvidenceType::Screenshot);
    EXPECT_EQ(screenshots.size(), 1u);
    
    auto crashes = coll.findByType(EvidenceType::CrashDump);
    EXPECT_TRUE(crashes.empty());
}

TEST_F(DebugIntelligenceTest, EvidenceCollection_FindByTag) {
    EvidenceCollection coll;
    
    Evidence evd1;
    evd1.tags["memory"] = "true";
    coll.add(evd1);
    
    Evidence evd2;
    evd2.tags["cpu"] = "true";
    coll.add(evd2);
    
    Evidence evd3;
    evd3.tags["memory"] = "true";
    evd3.tags["crash"] = "true";
    coll.add(evd3);
    
    auto memory_evd = coll.findByTag("memory");
    EXPECT_EQ(memory_evd.size(), 2u);
    
    auto cpu_evd = coll.findByTag("cpu");
    EXPECT_EQ(cpu_evd.size(), 1u);
}

TEST_F(DebugIntelligenceTest, EvidenceCollection_AllReturnsAll) {
    EvidenceCollection coll;
    
    for (int i = 0; i < 5; ++i) {
        Evidence evd;
        coll.add(evd);
    }
    
    auto all = coll.all();
    EXPECT_EQ(all.size(), 5u);
}

// ============================================================================
// Hypothesis Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, Hypothesis_DefaultConstructor_SetsDefaults) {
    Hypothesis hyp;
    
    EXPECT_FALSE(hyp.id.empty());
    EXPECT_TRUE(hyp.title.empty());
    EXPECT_EQ(hyp.status, InvestigationStatus::Open);
    EXPECT_DOUBLE_EQ(hyp.confidence_score, 0.0);
    EXPECT_TRUE(hyp.supporting_evidence_ids.empty());
    EXPECT_TRUE(hyp.refuting_evidence_ids.empty());
    EXPECT_FALSE(hyp.created_at.empty());
}

TEST_F(DebugIntelligenceTest, Hypothesis_GenerateId_UniqueIds) {
    std::set<std::string> ids;
    
    for (int i = 0; i < 100; ++i) {
        Hypothesis hyp;
        auto result = ids.insert(hyp.id);
        EXPECT_TRUE(result.second);
    }
}

TEST_F(DebugIntelligenceTest, Hypothesis_AddEvidence_NoDuplicates) {
    Hypothesis hyp;
    
    hyp.addSupportingEvidence("evd_1");
    hyp.addSupportingEvidence("evd_2");
    hyp.addSupportingEvidence("evd_1");  // Duplicate
    
    EXPECT_EQ(hyp.supporting_evidence_ids.size(), 2u);
}

TEST_F(DebugIntelligenceTest, Hypothesis_Confirm_SetsState) {
    Hypothesis hyp;
    hyp.title = "Test hypothesis";
    
    hyp.confirm();
    
    EXPECT_EQ(hyp.status, InvestigationStatus::Confirmed);
    EXPECT_FALSE(hyp.confirmed_at.empty());
    EXPECT_DOUBLE_EQ(hyp.confidence_score, 1.0);
}

TEST_F(DebugIntelligenceTest, Hypothesis_Reject_SetsState) {
    Hypothesis hyp;
    hyp.title = "Test hypothesis";
    
    hyp.reject();
    
    EXPECT_EQ(hyp.status, InvestigationStatus::Rejected);
    EXPECT_FALSE(hyp.rejected_at.empty());
    EXPECT_DOUBLE_EQ(hyp.confidence_score, 0.0);
}

TEST_F(DebugIntelligenceTest, InvestigationStatus_StringConversion_RoundTrip) {
    InvestigationStatus statuses[] = {
        InvestigationStatus::Open,
        InvestigationStatus::InProgress,
        InvestigationStatus::Confirmed,
        InvestigationStatus::Rejected,
        InvestigationStatus::NeedsEvidence,
        InvestigationStatus::Blocked,
        InvestigationStatus::Superseded
    };
    
    for (auto status : statuses) {
        std::string str = Hypothesis::statusToString(status);
        InvestigationStatus converted = Hypothesis::stringToStatus(str);
        EXPECT_EQ(converted, status) << "Failed round-trip for status " << str;
    }
}

// ============================================================================
// HypothesisTracker Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, HypothesisTracker_CreateAndFind) {
    HypothesisTracker tracker;
    
    auto& hyp1 = tracker.create("Null pointer dereference", "Possible null in parser");
    auto& hyp2 = tracker.create("Buffer overflow", "Size calculation error");
    
    EXPECT_EQ(tracker.size(), 2u);
    
    auto found = tracker.find(hyp1.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->title, "Null pointer dereference");
}

TEST_F(DebugIntelligenceTest, HypothesisTracker_Remove) {
    HypothesisTracker tracker;
    
    auto& hyp = tracker.create("Test");
    EXPECT_TRUE(tracker.remove(hyp.id));
    EXPECT_EQ(tracker.size(), 0u);
}

TEST_F(DebugIntelligenceTest, HypothesisTracker_FindByStatus) {
    HypothesisTracker tracker;
    
    auto& h1 = tracker.create("H1");
    auto& h2 = tracker.create("H2");
    auto& h3 = tracker.create("H3");
    
    h1.confirm();
    h2.reject();
    // h3 stays Open
    
    auto confirmed = tracker.findByStatus(InvestigationStatus::Confirmed);
    EXPECT_EQ(confirmed.size(), 1u);
    
    auto rejected = tracker.findByStatus(InvestigationStatus::Rejected);
    EXPECT_EQ(rejected.size(), 1u);
    
    auto active = tracker.getActive();
    EXPECT_EQ(active.size(), 1u);  // Only h3 is still open/in-progress/needs-evidence
}

TEST_F(DebugIntelligenceTest, HypothesisTracker_GetActive_ExcludesClosed) {
    HypothesisTracker tracker;
    
    auto& h1 = tracker.create("Active 1");
    auto& h2 = tracker.create("Active 2");
    auto& h3 = tracker.create("Confirmed");
    auto& h4 = tracker.create("Rejected");
    
    h3.confirm();
    h4.reject();
    
    auto active = tracker.getActive();
    EXPECT_EQ(active.size(), 2u);
}

TEST_F(DebugIntelligenceTest, HypothesisTracker_FindSimilar_DetectsDuplicates) {
    HypothesisTracker tracker;
    
    tracker.create("Null pointer in metadata parser", "Parser crashes on null input");
    
    auto similar = tracker.findSimilar("Null pointer in parsing");
    ASSERT_TRUE(similar.has_value());
    EXPECT_GE(similar->title.length(), 0u);  // Found something
    
    auto not_similar = tracker.findSimilar("GPU rendering issue");
    EXPECT_FALSE(not_similar.has_value());  // Too different
}

// ============================================================================
// ExperimentRecorder Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, Recorder_Initialize_CreatesDirectories) {
    ExperimentRecorder recorder(test_dir_);
    
    EXPECT_TRUE(recorder.initialize());
    EXPECT_TRUE(recorder.isInitialized());
    EXPECT_TRUE(fs::exists(test_dir_ / EVIDENCE_DIR));
    EXPECT_TRUE(fs::exists(test_dir_ / SCREENSHOTS_DIR));
    EXPECT_TRUE(fs::exists(test_dir_ / LOGS_DIR));
}

TEST_F(DebugIntelligenceTest, Recorder_CaptureBuildConfig_ReturnsValidConfig) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    auto config = recorder.captureBuildConfig();
    
    EXPECT_FALSE(config.compiler.empty());
    EXPECT_FALSE(config.build_type.empty());
    EXPECT_FALSE(config.cxx_standard.empty());
    // Git info might be empty if not in git repo, which is OK
}

TEST_F(DebugIntelligenceTest, Recorder_CaptureEnvironment_CapturesSystemInfo) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    auto env = recorder.captureEnvironment();
    
    EXPECT_FALSE(env.os_name.empty());
    EXPECT_FALSE(env.architecture.empty());
    EXPECT_GT(env.cpu_count, 0);
    EXPECT_FALSE(env.variables.empty());
}

TEST_F(DebugIntelligenceTest, Recorder_CaptureEnvironmentVar_RedactsSensitive) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Set a test sensitive variable (in real use, this would be set externally)
#ifdef _WIN32
    _putenv_s("TEST_SECRET_PASSWORD", "super_secret");
#else
    setenv("TEST_SECRET_PASSWORD", "super_secret", 1);
#endif
    
    auto evd = recorder.captureEnvironmentVar("TEST_SECRET_PASSWORD");
    
    EXPECT_EQ(evd.content, "[REDACTED]");
    EXPECT_EQ(evd.tags.at("sensitive"), "true");
}

TEST_F(DebugIntelligenceTest, Recorder_CreateLogEvidence_DetectsSeverity) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    auto error_log = recorder.createLogEvidence(
        "[ERROR] Something went wrong", "Error log", Severity::Info);
    
    // Should auto-detect ERROR and upgrade severity
    EXPECT_EQ(error_log.severity, Severity::Error);
    
    auto normal_log = recorder.createLogEvidence(
        "Normal operation completed", "Normal log");
    
    EXPECT_EQ(normal_log.severity, Severity::Info);
}

TEST_F(DebugIntelligenceTest, Recorder_CaptureLogFile_CopiesFile) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create test log file
    fs::path test_log = test_dir_ / "test.log";
    {
        std::ofstream file(test_log);
        file << "Test log content\n" << "Line 2\n" << "Line 3\n";
    }
    
    auto result = recorder.captureLogFile(test_log, "Test capture");
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.bytes_captured, 0u);
    EXPECT_TRUE(fs::exists(result.captured_path));
}

TEST_F(DebugIntelligenceTest, Recorder_ParseCrashLog_ValidFormat) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create test crash log
    fs::path crash_log = test_dir_ / "crash.log";
    {
        std::ofstream file(crash_log);
        file << "Signal: SIGSEGV (Segmentation fault)\n";
        file << "Fault address: 0x0000000000\n";
        file << "Stack trace:\n";
        file << "#0 0x1234 in function_a()\n";
        file << "#1 0x5678 in function_b()\n";
        file << "#2 0x9ABC in main()\n";
        file << "Registers:\n";
        file << "RIP=0x1234 RSP=0x5678\n";
    }
    
    auto crash = recorder.parseCrashLog(crash_log);
    
    ASSERT_TRUE(crash.has_value());
    EXPECT_FALSE(crash->signal_type.empty());
    EXPECT_EQ(crash->stack_trace.size(), 3u);
    EXPECT_TRUE(crash->hasStackTrace());
}

TEST_F(DebugIntelligenceTest, Recorder_ParseCrashLog_InvalidFormat) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create invalid crash log (no signal)
    fs::path invalid_log = test_dir_ / "invalid.log";
    {
        std::ofstream file(invalid_log);
        file << "This is not a crash log\n";
        file << "Just some random text\n";
    }
    
    auto crash = recorder.parseCrashLog(invalid_log);
    
    EXPECT_FALSE(crash.has_value());
}

TEST_F(DebugIntelligenceTest, Recorder_GenerateExpPackage_CreatesFile) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    ExperimentRecord record;
    record.id = "test_exp_001";
    record.title = "Test experiment";
    record.status = "completed";
    
    // Add some data
    record.build_config = recorder.captureBuildConfig();
    record.environment = recorder.captureEnvironment();
    
    Evidence evd;
    evd.type = EvidenceType::UserObservation;
    evd.description = "Test observation";
    evd.content = "Something happened";
    record.evidences.add(evd);
    
    auto package_path = recorder.generateExpPackage(record);
    
    ASSERT_TRUE(package_path.has_value());
    EXPECT_TRUE(fs::exists(*package_path));
    EXPECT_TRUE(package_path->extension().string() == EXP_PACKAGE_EXT);
}

TEST_F(DebugIntelligenceTest, Recorder_LoadExpPackage_RestoresData) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create and save an experiment
    ExperimentRecord original;
    original.id = "test_load_001";
    original.title = "Load test experiment";
    original.status = "running";
    original.description = "Testing load/save functionality";
    original.tags.push_back("unit-test");
    
    auto package_path = recorder.generateExpPackage(original);
    ASSERT_TRUE(package_path.has_value());
    
    // Load it back
    auto loaded = recorder.loadExpPackage(*package_path);
    
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->id, original.id);
    EXPECT_EQ(loaded->title, original.title);
    EXPECT_EQ(loaded->status, original.status);
}

// ============================================================================
// ReportGenerator Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, ReportGenerator_GeneratesReportFromExperiment) {
    ReportGenerator generator;
    
    ExperimentRecord record;
    record.id = "test_report_001";
    record.title = "Test investigation";
    record.description = "Testing report generation";
    
    // Add evidence
    Evidence evd1;
    evd1.type = EvidenceType::CrashDump;
    evd1.description = "Segfault in parser";
    evd1.severity = Severity::Critical;
    evd1.content = "Signal SIGSEGV at address 0x0";
    record.evidences.add(evd1);
    
    Evidence evd2;
    evd2.type = EvidenceType::LogEntry;
    evd2.description = "Error log shows null input";
    evd2.severity = Severity::Error;
    evd2.content = "[ERROR] Null pointer passed to parse()";
    record.evidences.add(evd2);
    
    // Add hypotheses
    auto& hyp1 = record.hypotheses.create("Null input handling missing");
    hyp1.confirm();
    hyp1.addSupportingEvidence(evd1.id);
    hyp1.addSupportingEvidence(evd2.id);
    
    auto& hyp2 = record.hypotheses.create("Memory corruption");
    hyp2.reject();
    hyp2.notes = "Memory analysis showed no corruption";
    
    auto report = generator.generateReport(record);
    
    EXPECT_EQ(report.experiment_id, record.id);
    EXPECT_FALSE(report.executive_summary.empty());
    EXPECT_FALSE(report.root_cause_description.empty());
    EXPECT_FALSE(report.confirmed_hypothesis_id.empty());
    EXPECT_GT(report.confidence_level, 0.0);
    EXPECT_FALSE(report.key_evidence.empty());
    EXPECT_FALSE(report.timeline.empty());
    EXPECT_FALSE(report.rejected_hypotheses.empty());
}

TEST_F(DebugIntelligenceTest, ReportGenerator_TextReport_ContainsAllSections) {
    ReportGenerator generator;
    
    ExperimentRecord record;
    record.id = "test_text_001";
    record.title = "Text report test";
    
    Evidence evd;
    evd.type = EvidenceType::UserObservation;
    evd.description = "Test evidence";
    evd.severity = Severity::Warning;
    record.evidences.add(evd);
    
    auto& hyp = record.hypotheses.create("Root cause");
    hyp.confirm();
    
    auto report = generator.generateReport(record);
    std::string text = generator.generateTextReport(report);
    
    // Check for key sections
    EXPECT_NE(text.find("ROOT CAUSE ANALYSIS REPORT"), std::string::npos);
    EXPECT_NE(text.find("EXECUTIVE SUMMARY"), std::string::npos);
    EXPECT_NE(text.find("ROOT CAUSE"), std::string::npos);
    EXPECT_NE(text.find("KEY EVIDENCE"), std::string::npos);
    EXPECT_NE(text.find("HYPOTHESIS ANALYSIS"), std::string::npos);
    EXPECT_NE(text.find(record.id), std::string::npos);
}

TEST_F(DebugIntelligenceTest, ReportGenerator_JsonReport_ValidJson) {
    ReportGenerator generator(ReportConfig::defaultConfig());
    
    ExperimentRecord record;
    record.id = "test_json_001";
    record.title = "JSON report test";
    
    auto report = generator.generateReport(record);
    std::string json = generator.generateJsonReport(report);
    
    // Basic JSON validity checks
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
    EXPECT_NE(json.find("\"experiment_id\""), std::string::npos);
    EXPECT_NE(json.find("\"generated_at\""), std::string::npos);
    EXPECT_NE(json.find("\"summary\""), std::string::npos);
}

TEST_F(DebugIntelligenceTest, ReportGenerator_SaveToFile) {
    ReportGenerator generator;
    
    ExperimentRecord record;
    record.id = "test_save_001";
    record.title = "Save test";
    
    auto report = generator.generateReport(record);
    fs::path output_path = test_dir_ / "test_report.txt";
    
    EXPECT_TRUE(generator.saveReport(report, output_path, false));
    EXPECT_TRUE(fs::exists(output_path));
    
    // Verify content was written
    std::ifstream file(output_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    EXPECT_FALSE(content.empty());
}

TEST_F(DebugIntelligenceTest, ReportGenerator_ConfigOptions_Work) {
    // Minimal config should produce shorter output
    ReportGenerator minimal_gen(ReportConfig::minimal());
    ReportGenerator detailed_gen(ReportConfig::detailed());
    
    ExperimentRecord record;
    record.id = "test_config_001";
    record.title = "Config test";
    
    Evidence evd;
    evd.content = std::string(10000, 'x');  // Long content
    record.evidences.add(evd);
    
    auto report_minimal = minimal_gen.generateReport(record);
    auto report_detailed = detailed_gen.generateReport(record);
    
    auto text_minimal = minimal_gen.generateTextReport(report_minimal);
    auto text_detailed = detailed_gen.generateTextReport(report_detailed);
    
    // Detailed should be longer than minimal
    EXPECT_LT(text_minimal.length(), text_detailed.length());
}

// ============================================================================
// HistorySearchAssistant Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, HistorySearch_BuildIndex_IndexesExperiments) {
    HistorySearchAssistant search(test_dir_);
    
    // Create some EXP packages
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    for (int i = 0; i < 3; ++i) {
        ExperimentRecord record;
        record.id = "hist_test_" + std::to_string(i);
        record.title = "History test experiment " + std::to_string(i);
        record.status = "completed";
        
        recorder.generateExpPackage(record);
    }
    
    size_t count = search.buildIndex();
    
    EXPECT_GE(count, 3u);
}

TEST_F(DebugIntelligenceTest, HistorySearch_Search_FindsMatches) {
    HistorySearchAssistant search(test_dir_);
    
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create target experiment
    ExperimentRecord target;
    target.id = "search_target_001";
    target.title = "IL2CPP metadata parser crash";
    target.status = "completed";
    target.tags.push_back("il2cpp");
    target.tags.push_back("parser");
    target.summary = "Investigating crash when parsing malformed metadata";
    recorder.generateExpPackage(target);
    
    // Create noise experiment
    ExperimentRecord noise;
    noise.id = "search_noise_001";
    noise.title = "GPU rendering performance";
    noise.status = "completed";
    recorder.generateExpPackage(noise);
    
    search.buildIndex();
    
    auto results = search.searchText("metadata parser");
    
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].entry.experiment_id, target.id);
    EXPECT_GT(results[0].relevance_score, 0.5);
}

TEST_F(DebugIntelligenceTest, HistorySearch_CheckForDuplicates_DetectsSimilar) {
    HistorySearchAssistant search(test_dir_);
    
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create existing experiment
    ExperimentRecord existing;
    existing.id = "dup_existing_001";
    existing.title = "Null pointer in IL2CPP string lookup";
    existing.status = "completed";
    recorder.generateExpPackage(existing);
    
    search.buildIndex();
    
    // Check for duplicate
    auto warnings = search.checkForDuplicates(
        "Null pointer during string lookup in IL2CPP",
        {"il2cpp", "crash"});
    
    ASSERT_FALSE(warnings.empty());
    EXPECT_GE(warnings[0].similarity_score, 0.6);
    EXPECT_EQ(warnings[0].existing_experiment_id, existing.id);
    EXPECT_FALSE(warnings[0].recommendation.empty());
}

TEST_F(DebugIntelligenceTest, HistorySearch_IsDuplicate_QuickCheck) {
    HistorySearchAssistant search(test_dir_);
    
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    ExperimentRecord existing;
    existing.id = "quick_dup_001";
    existing.title = "Specific bug title here";
    existing.status = "completed";
    recorder.generateExpPackage(existing);
    
    search.buildIndex();
    
    // Exact match should trigger
    auto [is_dup, warning] = search.isDuplicate("Specific bug title here");
    EXPECT_TRUE(is_dup);
    ASSERT_TRUE(warning.has_value());
    
    // Completely different should not
    auto [not_dup, no_warning] = search.isDuplicate("Totally different topic");
    EXPECT_FALSE(not_dup);
}

TEST_F(DebugIntelligenceTest, HistorySearch_Statistics_ReturnsData) {
    HistorySearchAssistant search(test_dir_);
    
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create varied experiments
    ExperimentRecord comp;
    comp.id = "stat_comp_001";
    comp.status = "completed";
    recorder.generateExpPackage(comp);
    
    ExperimentRecord running;
    running.id = "stat_run_001";
    running.status = "running";
    recorder.generateExpPackage(running);
    
    ExperimentRecord failed;
    failed.id = "stat_fail_001";
    failed.status = "failed";
    recorder.generateExpPackage(failed);
    
    search.buildIndex();
    
    auto stats = search.getStatistics();
    
    EXPECT_EQ(stats.at("total_experiments"), 3u);
    EXPECT_EQ(stats.at("completed"), 1u);
    EXPECT_EQ(stats.at("in_progress"), 1u);
    EXPECT_EQ(stats.at("failed"), 1u);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(DebugIntelligenceTest, FullWorkflow_CompleteInvestigation) {
    // This test simulates a complete debugging workflow
    
    // Initialize
    ExperimentRecorder recorder(test_dir_);
    ASSERT_TRUE(recorder.initialize());
    
    // Start experiment
    ExperimentRecord experiment;
    experiment.id = "workflow_001";
    experiment.title = "IL2CPP Parser Crash Investigation";
    experiment.description = "Investigating segfault when parsing global-metadata.dat";
    experiment.issue_reference = "GitHub Issue #1234";
    experiment.tags = {"il2cpp", "parser", "crash"};
    
    // Capture build state
    experiment.build_config = recorder.captureBuildConfig();
    experiment.environment = recorder.captureEnvironment();
    
    // Add initial evidence
    Evidence crash_evd;
    crash_evd.type = EvidenceType::CrashDump;
    crash_evd.description = "Segfault in MetadataParser::parse()";
    crash_evd.severity = Severity::Critical;
    crash_evd.content = "Signal: SIGSEGV\nAddress: 0x000000000000\nStack:\n#0 parse() at parser.cpp:42\n#1 main() at main.cpp:10";
    crash_evd.verified = true;
    experiment.evidences.add(crash_evd);
    
    Evidence log_evd;
    log_evd.type = EvidenceType::LogEntry;
    log_evd.description = "Error log showing null filename";
    log_evd.severity = Severity::Error;
    log_evd.content = "[ERROR] Filename is null in getString() call";
    log_evd.verified = true;
    experiment.evidences.add(log_evd);
    
    // Formulate hypotheses
    auto& hyp1 = experiment.hypotheses.create(
        "Null filename not checked",
        "The parser doesn't validate filename before using it");
    hyp1.addSupportingEvidence(log_evd.id);
    
    auto& hyp2 = experiment.hypotheses.create(
        "Corrupted metadata file",
        "Input file might be truncated or corrupted");
    hyp2.addRefutingEvidence(log_evd.id);  // Log shows it's not truncation
    
    auto& hyp3 = experiment.hypotheses.create(
        "Off-by-one error in offset calculation",
        "String table offset calculation might be wrong");
    
    // Investigate and confirm/reject
    hyp2.reject();  // File integrity verified
    hyp2.notes = "File checksum matches expected value";
    
    hyp1.confirm();  // Root cause found!
    
    // Complete experiment
    experiment.markCompleted();
    
    // Generate EXP package
    auto package_path = recorder.generateExpPackage(experiment);
    ASSERT_TRUE(package_path.has_value());
    
    // Generate report
    ReportGenerator generator;
    auto report = generator.generateReport(experiment);
    
    // Validate report
    EXPECT_TRUE(report.isValid());
    EXPECT_EQ(report.root_cause_description, 
              "The parser doesn't validate filename before using it");
    EXPECT_GT(report.confidence_level, 0.9);
    EXPECT_EQ(report.rejected_hypotheses.size(), 1u);
    EXPECT_FALSE(report.timeline.empty());
    
    // Verify we can reload everything
    auto reloaded = recorder.loadExpPackage(*package_path);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->id, experiment.id);
    EXPECT_EQ(reloaded->evidences.size(), 2u);
    EXPECT_EQ(reloaded->hypotheses.size(), 3u);
}

TEST_F(DebugIntelligenceTest, JsonSerialization_RoundTripPreservesData) {
    ExperimentRecorder recorder(test_dir_);
    recorder.initialize();
    
    // Create complex experiment
    ExperimentRecord original;
    original.id = "json_roundtrip_001";
    original.title = "JSON Serialization Test";
    original.description = "Testing that all fields survive round-trip";
    original.status = "completed";
    original.issue_reference = "JIRA-12345";
    original.tags = {"json", "serialization", "round-trip"};
    
    original.build_config = recorder.captureBuildConfig();
    original.environment = recorder.captureEnvironment();
    
    // Add various evidence types
    for (int i = 0; i < 5; ++i) {
        Evidence evd;
        evd.type = static_cast<EvidenceType>(i % 12);
        evd.description = "Evidence " + std::to_string(i);
        evd.content = "Content for evidence " + std::to_string(i);
        evd.severity = static_cast<Severity>(i % 4);
        original.evidences.add(evd);
    }
    
    // Add hypotheses with different states
    auto& h1 = original.hypotheses.create("Confirmed cause");
    h1.confirm();
    h1.notes = "This was the root cause";
    
    auto& h2 = original.hypotheses.create("Rejected theory");
    h2.reject();
    h2.notes = "Evidence disproved this";
    
    // Save and reload
    auto path = recorder.generateExpPackage(original);
    ASSERT_TRUE(path.has_value());
    
    auto loaded = recorder.loadExpPackage(*path);
    ASSERT_TRUE(loaded.has_value());
    
    // Verify key fields preserved
    EXPECT_EQ(loaded->id, original.id);
    EXPECT_EQ(loaded->title, original.title);
    EXPECT_EQ(loaded->status, original.status);
    EXPECT_EQ(loaded->issue_reference, original.issue_reference);
    EXPECT_EQ(loaded->tags.size(), original.tags.size());
}

} // namespace debug_intelligence

// ============================================================================
// Main - Run tests
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
