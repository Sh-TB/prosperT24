// test_diagnostics_disabled.cpp — Disabled-path compile test
//
// Verifies the diagnostics plugin API contract when PROSPER_DIAGNOSTICS is NOT defined.
// This test validates:
//   1. diagnostics.hpp compiles without PROSPER_DIAGNOSTICS
//   2. PluginInfo is visible and constructible (outside #ifdef)
//   3. register_plugin(const PluginInfo&) compiles with exact signature (no ellipsis)
//   4. register_plugin returns false in disabled mode
//   5. plugin_count() returns 0 in disabled mode
//
// Build: c++ -std=c++17 -I../prosper/src test_diagnostics_disabled.cpp ../prosper/src/host/diagnostics.cpp -o test_diagnostics_disabled
// Run:   ./test_diagnostics_disabled
// Exit:  0 = PASS, 1 = FAIL

#include "host/diagnostics.hpp"
#include <cstdio>
#include <cstdlib>

// IMPORTANT: Do NOT define PROSPER_DIAGNOSTICS — this tests the DISABLED path
#ifdef PROSPER_DIAGNOSTICS
#error "This test MUST be compiled WITHOUT PROSPER_DIAGNOSTICS defined"
#endif

int main() {
    printf("[test:diagnostics-disabled] Starting disabled-path contract test\n");
    
    int failures = 0;
    
    // --- Test 1: PluginInfo is constructible (visible outside #ifdef) ---
    printf("[test:diagnostics-disabled] Test 1: Construct PluginInfo... ");
    prosper::PluginInfo info{
        "test_plugin",
        "1.0",
        "Test plugin for disabled-mode verification"
    };
    printf("PASS\n");
    
    // --- Test 2: plugin_registry() returns a usable object ---
    printf("[test:diagnostics-disabled] Test 2: Access plugin_registry()... ");
    auto& registry = prosper::plugin_registry();
    (void)registry;  // Suppress unused warning
    printf("PASS\n");
    
    // --- Test 3: register_plugin accepts const PluginInfo& (exact signature) ---
    printf("[test:diagnostics-disabled] Test 3: Call register_plugin(const PluginInfo&)... ");
    bool result = prosper::plugin_registry().register_plugin(info);
    if (result != false) {
        printf("FAIL (expected false, got true)\n");
        failures++;
    } else {
        printf("PASS (returned false as expected)\n");
    }
    
    // --- Test 4: plugin_count() returns 0 after rejected registration ---
    printf("[test:diagnostics-disabled] Test 4: plugin_count() == 0... ");
    size_t count = prosper::plugin_registry().plugin_count();
    if (count != 0) {
        printf("FAIL (expected 0, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS (count = 0)\n");
    }
    
    // --- Test 5: Multiple registrations all fail ---
    printf("[test:diagnostics-disabled] Test 5: Multiple registrations all return false... ");
    prosper::PluginInfo info2{"another", "2.0", "Second test plugin"};
    prosper::PluginInfo info3{"third", "3.0", "Third test plugin"};
    
    if (prosper::plugin_registry().register_plugin(info2) != false ||
        prosper::plugin_registry().register_plugin(info3) != false) {
        printf("FAIL (expected all false)\n");
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Test 6: Count still 0 after multiple attempts ---
    printf("[test:diagnostics-disabled] Test 6: plugin_count() still 0... ");
    count = prosper::plugin_registry().plugin_count();
    if (count != 0) {
        printf("FAIL (expected 0, got %zu)\n", count);
        failures++;
    } else {
        printf("PASS\n");
    }
    
    // --- Summary ---
    printf("\n[test:diagnostics-disabled] Results: %d failures\n", failures);
    if (failures == 0) {
        printf("[test:diagnostics-disabled] ALL TESTS PASSED\n");
        return 0;  // EXIT_SUCCESS
    } else {
        printf("[test:diagnostics-disabled] SOME TESTS FAILED\n");
        return 1;  // EXIT_FAILURE
    }
}
