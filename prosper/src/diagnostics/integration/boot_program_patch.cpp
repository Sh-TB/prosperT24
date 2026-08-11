// diagnostics/integration/boot_program_patch.cpp — Example integration into boot_program()
//
// This file shows how to add diagnostics hooks to boot_program.cpp.
// The actual integration would be done by editing boot_program.cpp directly
// to include these hook calls at the appropriate points.
//
// Each hook is wrapped in: if (diagnostics_enabled) { ... }
// So there is ZERO overhead when diagnostics are disabled.

#include "integration/boot_integration.hpp"

namespace prosper {
namespace diagnostics {

// This function demonstrates where hooks would be placed in boot_program().
// In practice, these calls would be added directly to boot_program.cpp.

void example_boot_program_with_diagnostics(
    const std::string& d,
    Program& p,
    std::string* err,
    const std::function<void()>& after_hle_registered
) {
    auto fail = [&](const std::string& m) { if (err) *err = m; return false; };

    // === HOOK: Boot Start ===
    BootIntegration::on_boot_start(d);

    // libc.prx loaded last => its init_array runs first (deepest dependency)
    std::vector<LinkInput> in = boot_link_inputs(d);

    // === HOOK: Link Start ===
    BootIntegration::on_link_start(in.size());

    std::string e;
    if (!link_program(in, BOOT_STUB, p, &e)) {
        // === HOOK: Link Failed ===
        if (DiagnosticContext::instance().is_enabled()) {
            BootIntegration::on_boot_failed("link failed: " + e, BootPhase::ELF_PARSED);
        }
        return fail("link failed: " + e);
    }

    // === HOOK: Link Complete ===
    BootIntegration::on_link_complete(p);

    // [Existing code: skipped_modules, aliased_reports, printf...]
    
    sceKernelDlsym setup...
    register_builtin_hle();
    
    // === HOOK: Import Resolution ===
    auto* prx_col = DiagnosticsIntegration::prx();
    BootIntegration::on_import_resolution_complete(prx_col);

    if (after_hle_registered) after_hle_registered();

    set_app0_root(d);
    for (auto& img : p.imgs) if (!map_image(img, &e)) {
        if (DiagnosticContext::instance().is_enabled()) {
            BootIntegration::on_boot_failed("map failed: " + e, BootPhase::SEGMENTS_MAPPED);
        }
        return fail("map failed: " + e);
    }

    // === HOOK: Segments Mapped ===
    BootIntegration::on_segments_mapped();

    // TLS setup...
    // Unwind setup...
    // Proc param...

    // Stubs and trap handler
    p.slots.reserve(p.slots.size() + 1024);
    if (!install_stubs(p.slots, p.stub_base, p.stub_size, &e)) {
        if (DiagnosticContext::instance().is_enabled()) {
            BootIntegration::on_boot_failed("stubs failed: " + e, BootPhase::RELOCATIONS_APPLIED);
        }
        return fail("stubs failed: " + e);
    }
    install_trap_handler();

    // === HOOK: Relocations Complete ===
    BootIntegration::on_relocations_complete();

    runtime_module_loader_init(&p);
    // Module start params...
    
    // === HOOK: Init Start ===
    BootIntegration::on_init_start(p.init_fns.size());

    run_guest_inits(p.init_fns);

    // === HOOK: Init Complete / Boot Complete ===
    BootIntegration::on_init_complete(true);
    BootIntegration::on_boot_complete(p.entry);

    return true;
}

} // namespace diagnostics
} // namespace prosper

/*
ACTUAL INTEGRATION IN boot_program.cpp:

Add at the top of boot_program():
#include "diagnostics/diagnostics.hpp"  // (or include path as needed)

Then add these calls at the corresponding points:

1. After function start:
   DIAG_INTEGRATION_BOOT_START(d);

2. After boot_link_inputs():
   DIAG_INTEGRATION_LINK_START(in.size());

3. After successful link_program():
   DIAG_INTEGRATION_LINK_COMPLETE(p);

4. After all map_image() calls succeed:
   DIAG_INTEGRATION_SEGMENTS_MAPPED();

5. After register_builtin_hle():
   DIAG_INTEGRATION_IMPORT_RESOLUTION_COMPLETE();

6. After install_stubs() succeeds:
   DIAG_INTEGRATION_RELOCATIONS_COMPLETE();

7. Before run_guest_inits():
   DIAG_INTEGRATION_INIT_START(p.init_fns.size());

8. After run_guest_inits():
   DIAG_INTEGRATION_INIT_COMPLETE(true);  // or false on failure

9. Before final return true:
   DIAG_INTEGRATION_BOOT_COMPLETE(p.entry);

Each of these macros expands to nothing when diagnostics is disabled.
*/
