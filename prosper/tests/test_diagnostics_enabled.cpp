// test_diagnostics_enabled.cpp — Enabled-path compile test
//
// Verifies the diagnostics plugin API contract when PROSPER_DIAGNOSTICS IS defined.
// This test validates:
//   1. diagnostics.hpp compiles with PROSPER_DIAGNOSTICS
//   2. PluginInfo is visible and constructible
//   3. register_plugin(const PluginInfo&) accepts registration
//   4. register_plugin returns true on success
//   5. plugin_count() increments
//   6. Duplicate registration returns false
//
// Build: c++ -std=c++17 -DPROSPER_DIAGNOSTICS -I../prosper/src test_diagnostics_enabled.cpp ../prosper/src/host/diagnostics.cpp -o test_diagnostics_enabled
// Run:   ./test_diagnostics_enabled
// Exit:  0 = PASS, 1 = FAIL

#define PROSPER_DIAGNOSTICS
#include "host/diagnostics.hpp"
#include <cstdio>
#include <cstdlib>

int main() {
    printf("[test:diagnostics-enabled] Starting enabled-path contract test\n");
    
    int failures = 0;
    
    // --- Test 1: PluginInfo is constructible ---
    printf("[test:diagnostics-enabled] Test 1: Construct PluginInfo... ");
    prosper::PluginInfo info{
        "boot_state",
        "1.0",
        "Boot phase diagnostics"
    };
    printf("PASS\n");
    
    // --- Test 2: Initial state: empty registry ---
    printf("[test:diagnostics-enabled] Test 2: Initial plugin_count() == 0... ");
    size_t count = prosper::plugin_registry().plugin_count();
    if (count != 0) {
        printf("FAIL (expected 0, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 3: register_plugin succeeds ---
    printf("[test:diagnostics-enabled] Test 3: register_plugin returns true... ");
    bool result = prosper::plugin_registry().register_plugin(info);
    if (result != true) {
        printf("FAIL (expected true, got false)\n");
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 4: Count incremented ---
    printf("[test:diagnostics-enabled] Test 4: plugin_count() == 1... ");
    count = prosper::plugin_registry().plugin_count();
    if (count != 1) {
        printf("FAIL (expected 1, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 5: has_plugin finds registered plugin ---
    printf("[test:diagnostics-enabled] Test 5: has_plugin(\"boot_state\") == true... ");
    if (!prosper::plugin_registry().has_plugin("boot_state")) {
        printf("FAIL (expected true)\n");
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 6: Duplicate registration fails ---
    printf("[test:diagnostics-enabled] Test 6: Duplicate registration returns false... ");
    prosper::PluginInfo dup{"boot_state", "1.0", "Duplicate"};
    result = prosper::plugin_registry().register_plugin(dup);
    if (result != false) {
        printf("FAIL (expected false for duplicate)\n");
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 7: Count unchanged after duplicate ---
    printf("[test:diagnostics-enabled] Test 7: plugin_count() still 1 after duplicate... ");
    count = prosper::plugin_registry().plugin_count();
    if (count != 1) {
        printf("FAIL (expected 1, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 8: Second unique plugin succeeds ---
    printf("[test:diagnostics-enabled] Test 8: Second unique plugin registers... ");
    prosper::PluginInfo info2{"crash_context", "1.0", "Crash context diagnostics"};
    result = prosper::plugin_registry().register_plugin(info2);
    if (result != true) {
        printf("FAIL (expected true)\n");
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 9: Count now 2 ---
    printf("[test:diagnostics-enabled] Test 9: plugin_count() == 2... ");
    count = prosper::plugin_registry().plugin_count();
    if (count != 2) {
        printf("FAIL (expected 2, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Summary ---
    printf("\n[test:diagnostics-enabled] Results: %d failures\n", failures);
    if (failures == 0) {
        printf("[test:diagnostics-enabled] ALL TESTS PASSED\n");
        return 0;  // EXIT_SUCCESS
    } else {
        printf("[test:diagnostics-enabled] SOME TESTS FAILED\n");
        return 1;  // EXIT_FAILURE
    }
}
