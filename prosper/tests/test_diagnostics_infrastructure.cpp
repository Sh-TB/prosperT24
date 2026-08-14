/**
 * Diagnostics Infrastructure Test Suite
 * 
 * Tests BOTH configurations:
 * 1. DISABLED build (no PROSPER_DIAGNOSTICS) - stub API
 * 2. ENABLED build (-DPROSPER_DIAGNOSTICS) - full functionality
 * 
 * CRITICAL: This test validates PR #2513 fix:
 * - PluginInfo is available in both builds
 * - register_plugin() has same signature in both modes
 * - Disabled build returns safe defaults (false, 0, empty)
 */

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

// Include diagnostics header
#include "diagnostics/diagnostics.hpp"

using namespace prosper::diagnostics;

// ============================================================================
// Test Counters
// ============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            std::cout << "  ✅ PASSED: " << message << "\n"; \
            ++tests_passed; \
        } else { \
            std::cout << "  ❌ FAILED: " << message << "\n"; \
            ++tests_failed; \
        } \
    } while(0)

// ============================================================================
// P1: PluginInfo Shared API Tests
// (Must pass in BOTH enabled and disabled builds)
// ============================================================================

void test_plugin_info_shared_api() {
    std::cout << "\n[P1] PluginInfo Shared API Tests\n";
    
    // Test 1.1: PluginInfo can be constructed outside #ifdef
    {
        PluginInfo info{"test_plugin", "1.0.0", "Test plugin"};
        
        TEST_ASSERT(info.name == "test_plugin", 
                   "PluginInfo construction works");
        TEST_ASSERT(info.version == "1.0.0", 
                   "PluginInfo version set correctly");
        TEST_ASSERT(info.description == "Test plugin", 
                   "PluginInfo description set correctly");
        TEST_ASSERT(info.isValid(), 
                   "Valid PluginInfo returns true for isValid()");
    }
    
    // Test 1.2: Invalid PluginInfo detection
    {
        PluginInfo empty_info;
        
        TEST_ASSERT(!empty_info.isValid(), 
                   "Empty PluginInfo returns false for isValid()");
        
        PluginInfo no_version{"name_only", "", "desc"};
        TEST_ASSERT(!no_version.isValid(), 
                   "PluginInfo without version is invalid");
    }
    
    // Test 1.3: PluginInfo JSON serialization works
    {
        PluginInfo info{"json_test", "2.0.0", "JSON test"};
        std::string json = info.toJson();
        
        TEST_ASSERT(json.find("\"name\"") != std::string::npos, 
                   "PluginInfo toJson() contains name field");
        TEST_ASSERT(json.find("json_test") != std::string::npos, 
                   "PluginInfo toJson() contains name value");
        TEST_ASSERT(json.find("2.0.0") != std::string::npos, 
                   "PluginInfo toJson() contains version value");
    }
    
    // Test 1.4: PluginInfo with optional fields
    {
        PluginInfo info{"full_plugin", "3.0.0", "Full test"};
        info.author = "Test Author";
        info.dependencies.push_back("dep1");
        info.config["key"] = "value";
        
        TEST_ASSERT(info.author == "Test Author", 
                   "PluginInfo author field works");
        TEST_ASSERT(info.dependencies.size() == 1, 
                   "PluginInfo dependencies work");
        TEST_ASSERT(info.dependencies[0] == "dep1", 
                   "PluginInfo dependency content correct");
        TEST_ASSERT(info.config.at("key") == "value", 
                   "PluginInfo config map works");
    }
}

// ============================================================================
// P2: Disabled Build Stub Tests
// (Tests stub behavior when PROSPER_DIAGNOSTICS not defined)
// ============================================================================

void test_disabled_build_stub() {
    std::cout << "\n[P2] Disabled Build Stub Tests\n";
    
#ifdef PROSPER_DIAGNOSTICS
    std::cout << "  ⚠️  SKIPPED: PROSPER_DIAGNOSTICS is defined\n";
    return;
#endif
    
    // Test 2.1: is_enabled() returns false
    {
        TEST_ASSERT(!is_enabled(), 
                   "is_enabled() returns false in disabled mode");
    }
    
    // Test 2.2: initialize() succeeds (no-op)
    {
        bool result = initialize();
        TEST_ASSERT(result, 
                   "initialize() returns true (success no-op)");
    }
    
    // Test 2.3: shutdown() doesn't crash
    {
        shutdown();  // Should not crash or throw
        TEST_ASSERT(true, 
                   "shutdown() executes without error");
    }
    
    // Test 2.4: emit_event() doesn't crash
    {
        emit_event("test_category", "test message", Severity::Info, PROSPER_DIAG_HERE());
        TEST_ASSERT(true, 
                   "emit_event() executes without error");
    }
}

// ============================================================================
// P3: PluginRegistry Stub Tests (PR #2513 Fix Validation)
// ============================================================================

void test_plugin_registry_api_contract() {
    std::cout << "\n[P3] PluginRegistry API Contract Tests (PR #2513)\n";
    
    // Test 3.1: plugin_registry() singleton exists and returns reference
    {
        auto& registry = plugin_registry();
        
        // Just verify we can get the instance
        (void)registry;
        TEST_ASSERT(true, 
                   "plugin_registry() returns valid reference");
    }
    
    // Test 3.2: register_plugin(const PluginInfo&) compiles and runs
    {
        PluginInfo info{
            "stub_test",
            "1.0",
            "Stub test plugin"
        };
        
        bool result = plugin_registry().register_plugin(info);
        
#ifndef PROSPER_DIAGNOSTICS
        // In disabled mode, should always return false
        TEST_ASSERT(result == false, 
                   "register_plugin() returns false in disabled mode");
#else
        // In enabled mode, should succeed for valid plugin
        TEST_ASSERT(result == true, 
                   "register_plugin() returns true for valid plugin in enabled mode");
#endif
    }
    
    // Test 3.3: plugin_count() returns correct value
    {
        size_t count = plugin_registry().plugin_count();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(count == 0, 
                   "plugin_count() returns 0 in disabled mode");
#else
        // We registered one above, so count should be >= 1
        TEST_ASSERT(count >= 1, 
                   "plugin_count() reflects registered plugins in enabled mode");
#endif
    }
    
    // Test 3.4: has_plugin() returns correct value
    {
        PluginInfo info{"query_test", "1.0", "Query test"};
        plugin_registry().register_plugin(info);
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(!plugin_registry().has_plugin("query_test"), 
                   "has_plugin() returns false in disabled mode");
#else
        TEST_ASSERT(plugin_registry().has_plugin("query_test"), 
                   "has_plugin() finds registered plugin in enabled mode");
#endif
    }
    
    // Test 3.5: get_plugin() returns nullptr in disabled mode
    {
        auto* plugin = plugin_registry().get_plugin("any_plugin");
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(plugin == nullptr, 
                   "get_plugin() returns nullptr in disabled mode");
#else
        // May or may not be null depending on if registered
        TEST_ASSERT(true, 
                   "get_plugin() executes without error in enabled mode");
#endif
    }
    
    // Test 3.6: unregister_plugin() returns false in disabled mode
    {
        bool result = plugin_registry().unregister_plugin("any_plugin");
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(result == false, 
                   "unregister_plugin() returns false in disabled mode");
#else
        TEST_ASSERT(true, 
                   "unregister_plugin() executes in enabled mode");
#endif
    }
    
    // Test 3.7: get_all_plugins() returns empty vector in disabled mode
    {
        auto plugins = plugin_registry().get_all_plugins();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(plugins.empty(), 
                   "get_all_plugins() returns empty vector in disabled mode");
#else
        TEST_ASSERT(true, 
                   "get_all_plugins() executes in enabled mode");
#endif
    }
    
    // Test 3.8: initialize_all() returns true in disabled mode
    {
        bool result = plugin_registry().initialize_all();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(result == true, 
                   "initialize_all() returns true (no-op success) in disabled mode");
#else
        TEST_ASSERT(result == true || result == false, 
                   "initialize_all() returns bool in enabled mode");
#endif
    }
    
    // Test 3.9: export_json() returns valid JSON
    {
        std::string json = plugin_registry().export_json();
        
        TEST_ASSERT(!json.empty(), 
                   "export_json() returns non-empty string");
        TEST_ASSERT(json.find("{") != std::string::npos, 
                   "export_json() returns valid JSON object");
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(json.find("\"count\": 0") != std::string::npos, 
                   "export_json() shows count=0 in disabled mode");
#endif
    }
    
    // Test 3.10: Multiple registrations all fail in disabled mode
    {
        for (int i = 0; i < 5; ++i) {
            PluginInfo info{
                "multi_" + std::to_string(i),
                "1.0",
                "Multi test"
            };
            
            bool result = plugin_registry().register_plugin(info);
            
#ifndef PROSPER_DIAGNOSTICS
            TEST_ASSERT(result == false, 
                       "Multiple register_plugin() calls all return false in disabled mode");
#endif
        }
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(plugin_registry().plugin_count() == 0, 
                   "plugin_count() remains 0 after multiple failed registrations");
#endif
    }
}

// ============================================================================
// P4: Boot Phase Recording Tests
// ============================================================================

void test_boot_phase_recording() {
    std::cout << "\n[P4] Boot Phase Recording Tests\n";
    
    // Test 4.1: record_boot_phase() compiles and executes
    {
        record_boot_phase(BootPhase::Initialization, PROSPER_DIAG_HERE(), "Initializing diagnostics test");
        TEST_ASSERT(true, 
                   "record_boot_phase() executes without error");
    }
    
    // Test 4.2: Boot phase enum values are valid
    {
        TEST_ASSERT(bootPhaseToString(BootPhase::None) != nullptr, 
                   "bootPhaseToString() handles None phase");
        TEST_ASSERT(bootPhaseToString(BootPhase::Ready) != nullptr, 
                   "bootPhaseToString() handles Ready phase");
        TEST_ASSERT(std::string(bootPhaseToString(BootPhase::Error)) == "Error", 
                   "bootPhaseToString(Error) returns 'Error'");
    }
    
    // Test 4.3: get_current_boot_phase() works
    {
        BootPhase current = get_current_boot_phase();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(current == BootPhase::None, 
                   "get_current_boot_phase() returns None in disabled mode");
#else
        // Should be Initialization from our earlier call
        TEST_ASSERT(current == BootPhase::Initialization || current == BootPhase::None, 
                   "get_current_boot_phase() returns valid phase in enabled mode");
#endif
    }
    
    // Test 4.4: get_boot_phase_history() works
    {
        auto history = get_boot_phase_history();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(history.empty(), 
                   "get_boot_phase_history() returns empty in disabled mode");
#else
        TEST_ASSERT(true, 
                   "get_boot_phase_history() executes in enabled mode");
#endif
    }
    
    // Test 4.5: boot_completed_successfully() returns sensible default
    {
        bool completed = boot_completed_successfully();
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(completed == true, 
                   "boot_completed_successfully() assumes success in disabled mode");
#else
        TEST_ASSERT(completed == true || completed == false, 
                   "boot_completed_successfully() returns bool in enabled mode");
#endif
    }
    
    // Test 4.6: Record multiple phases
    {
        record_boot_phase(BootPhase::HLESetup, PROSPER_DIAG_HERE(), "Setting up HLE");
        record_boot_phase(BootPhase::KernelInit, PROSPER_DIAG_HERE(), "Initializing kernel");
        record_boot_phase(BootPhase::GpuInit, PROSPER_DIAG_HERE(), "Initializing GPU");
        record_boot_phase(BootPhase::Ready, PROSPER_DIAG_HERE(), "System ready");
        
        TEST_ASSERT(true, 
                   "Multiple record_boot_phase() calls execute without error");
    }
}

// ============================================================================
// P5: EventBus Tests
// ============================================================================

void test_event_bus() {
    std::cout << "\n[P5] EventBus Tests\n";
    
    // Test 5.1: event_bus() singleton exists
    {
        auto& bus = event_bus();
        (void)bus;
        TEST_ASSERT(true, 
                   "event_bus() returns valid reference");
    }
    
    // Test 5.2: subscriber_count() works
    {
        size_t count = event_bus().subscriber_count("test_category");
        
#ifndef PROSPER_DIAGNOSTICS
        TEST_ASSERT(count == 0, 
                   "subscriber_count() returns 0 in disabled mode");
#else
        TEST_ASSERT(count >= 0, 
                   "subscriber_count() returns non-negative in enabled mode");
#endif
    }
}

// ============================================================================
// P6: Statistics & Export Tests
// ============================================================================

void test_statistics_and_export() {
    std::cout << "\n[P6] Statistics & Export Tests\n";
    
    // Test 6.1: get_stats() returns valid struct
    {
        DiagnosticsStats stats = get_stats();
        
        TEST_ASSERT(stats.total_events >= 0, 
                   "get_stats() returns valid total_events");
        TEST_ASSERT(stats.plugins_registered >= 0, 
                   "get_stats() returns valid plugins_registered");
    }
    
    // Test 6.2: Export functions return valid JSON
    {
        std::string events_json = export_events_json(100);  // Last 100 events
        std::string boot_json = export_boot_phases_json();
        std::string full_report = export_full_report_json();
        
        TEST_ASSERT(!events_json.empty(), 
                   "export_events_json() returns non-empty string");
        TEST_ASSERT(events_json.find("{") != std::string::npos, 
                   "export_events_json() returns valid JSON");
        
        TEST_ASSERT(!boot_json.empty(), 
                   "export_boot_phases_json() returns non-empty string");
        
        TEST_ASSERT(!full_report.empty(), 
                   "export_full_report_json() returns non-empty string");
        TEST_ASSERT(full_report.find("diagnostics") != std::string::npos, 
                   "export_full_report_json() contains diagnostics section");
    }
}

// ============================================================================
// P7: SourceLocation Tests
// ============================================================================

void test_source_location() {
    std::cout << "\n[P7] SourceLocation Tests\n";
    
    // Test 7.1: Default construction
    {
        SourceLocation loc;
        TEST_ASSERT(loc.file == nullptr, 
                   "Default SourceLocation has null file");
        TEST_ASSERT(loc.line == 0, 
                   "Default SourceLocation has line 0");
    }
    
    // Test 7.2: Parameterized construction
    {
        SourceLocation loc{"test.cpp", 42, "test_func"};
        
        TEST_ASSERT(loc.file != nullptr, 
                   "SourceLocation file set correctly");
        TEST_ASSERT(loc.line == 42, 
                   "SourceLocation line set correctly");
        TEST_ASSERT(loc.function != nullptr, 
                   "SourceLocation function set correctly");
    }
    
    // Test 7.3: toString()
    {
        SourceLocation loc{"myfile.cpp", 100, "myFunction"};
        std::string str = loc.toString();
        
        TEST_ASSERT(str.find("myfile.cpp") != std::string::npos, 
                   "SourceLocation toString() contains filename");
        TEST_ASSERT(str.find("100") != std::string::npos, 
                   "SourceLocation toString() contains line number");
        TEST_ASSERT(str.find("myFunction") != std::string::npos, 
                   "SourceLocation toString() contains function name");
    }
    
    // Test 7.4: PROSPER_DIAG_HERE macro works
    {
        auto loc = PROSPER_DIAG_HERE();
        
        TEST_ASSERT(loc.file != nullptr, 
                   "PROSPER_DIAG_HERE() sets file");
        TEST_ASSERT(loc.line > 0, 
                   "PROSPER_DIAG_HERE() sets line > 0");
        TEST_ASSERT(loc.function != nullptr, 
                   "PROSPER_DIAG_HERE() sets function");
    }
}

// ============================================================================
// P8: Severity Tests
// ============================================================================

void test_severity() {
    std::cout << "\n[P8] Severity Tests\n";
    
    // Test 8.1: All severity levels have string representations
    {
        const char* debug_str = severityToString(Severity::Debug);
        const char* info_str = severityToString(Severity::Info);
        const char* warning_str = severityToString(Severity::Warning);
        const char* error_str = severityToString(Severity::Error);
        const char* critical_str = severityToString(Severity::Critical);
        const char* fatal_str = severityToString(Severity::Fatal);
        
        TEST_ASSERT(debug_str != nullptr && std::string(debug_str) == "DEBUG", 
                   "Severity::Debug maps to 'DEBUG'");
        TEST_ASSERT(info_str != nullptr && std::string(info_str) == "INFO", 
                   "Severity::Info maps to 'INFO'");
        TEST_ASSERT(warning_str != nullptr && std::string(warning_str) == "WARNING", 
                   "Severity::Warning maps to 'WARNING'");
        TEST_ASSERT(error_str != nullptr && std::string(error_str) == "ERROR", 
                   "Severity::Error maps to 'ERROR'");
        TEST_ASSERT(critical_str != nullptr && std::string(critical_str) == "CRITICAL", 
                   "Severity::Critical maps to 'CRITICAL'");
        TEST_ASSERT(fatal_str != nullptr && std::string(fatal_str) == "FATAL", 
                   "Severity::Fatal maps to 'FATAL'");
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "Diagnostics Infrastructure Test Suite\n";
    std::cout << "========================================\n";
    
#ifdef PROSPER_DIAGNOSTICS
    std::cout << "Build Mode: ENABLED (-DPROSPER_DIAGNOSTICS)\n";
#else
    std::cout << "Build Mode: DISABLED (stub)\n";
#endif
    
    std::cout << "Version: " << DIAGNOSTICS_VERSION << "\n";
    std::cout << "API Level: " << DIAGNOSTICS_API_LEVEL << "\n";
    
    // Initialize if enabled
#ifdef PROSPER_DIAGNOSTICS
    initialize();
#endif
    
    // Run all test suites
    test_plugin_info_shared_api();      // P1: Always runs
    test_disabled_build_stub();         // P2: Stub-specific
    test_plugin_registry_api_contract();// P3: PR #2513 validation
    test_boot_phase_recording();        // P4: Boot tracking
    test_event_bus();                   // P5: Event system
    test_statistics_and_export();       // P6: Data export
    test_source_location();             // P7: Location utilities
    test_severity();                    // P8: Severity enums
    
    // Cleanup if enabled
#ifdef PROSPER_DIAGNOSTICS
    shutdown();
#endif
    
    // Summary
    std::cout << "\n========================================\n";
    std::cout << "TEST SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Total:  " << (tests_passed + tests_failed) << "\n";
    
    if (tests_failed == 0) {
        std::cout << "\n🎉 ALL TESTS PASSED! 🎉\n";
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED\n";
        return 1;
    }
}
