// test_boot_linux — M3: jump into the real guest entry and let it run, with
// unimplemented imports logged-and-stubbed (return 0) so the boot advances. Reports
// the startup Sony-call sequence and where it finally faults. Success = the crt
// executed and made at least one Sony call (dispatch works end to end).
#include "../src/self/module.hpp"
#include "../src/host/exec_image.hpp"
#include "../src/hle/dispatch.hpp"
#include <cstdio>
#include <string>

using namespace prosper;

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "../../PPSA24651-app0/eboot.bin";
    printf("== test_boot_linux: %s ==\n", path);

    std::string err;
    auto mo = Module::load(path, &err);
    if (!mo) { printf("  [FAIL] load: %s\n", err.c_str()); return 1; }
    Module& m = *mo;

    const uint64_t BASE = 0x400000000ull, STUB_BASE = 0x500000000ull, STUB_SZ = 32;
    LoadedImage img = build_image(m, BASE);
    bind_imports_to_stubs(m, img, STUB_BASE, STUB_SZ);
    apply_relocations(m, img);
    if (!map_image(m, img, &err))               { printf("  [FAIL] map: %s\n", err.c_str()); return 1; }
    register_builtin_hle();                      // real libc thunks so the crt can make progress
    if (!install_stubs(m, STUB_BASE, STUB_SZ, &err)) { printf("  [FAIL] stubs: %s\n", err.c_str()); return 1; }
    install_trap_handler();
    reset_call_log();

    printf("  entry = 0x%llx  jumping in...\n\n", (unsigned long long)img.entry);
    BootResult r = run_entry(img);

    dump_call_log(stdout);
    printf("\n  run ended: kind=%d  %s\n", r.kind, r.detail.c_str());
    printf("  regs @fault: rbp=0x%llx rsp=0x%llx rax=0x%llx rdi=0x%llx rsi=0x%llx rdx=0x%llx\n",
           (unsigned long long)r.rbp, (unsigned long long)r.rsp, (unsigned long long)r.rax,
           (unsigned long long)r.rdi, (unsigned long long)r.rsi, (unsigned long long)r.rdx);

    // Regression guard: correct stack alignment lets the crt clear its SIMD static-init
    // and reach libkernel thread/mutex initialization. If alignment regresses, the boot
    // dies in the SIMD init and never calls into libkernel.
    size_t n = call_order().size();
    bool reached_libkernel = false;
    for (uint32_t idx : call_order())
        if (idx < m.imports.size() && m.imports[idx].lib_name == "libkernel") reached_libkernel = true;
    if (n >= 1 && reached_libkernel) {
        printf("\n== PASS: guest ran through crt/global-init into libkernel (%zu distinct calls) ==\n", n);
        return 0;
    }
    printf("\n== FAIL: boot did not reach libkernel init (n=%zu, libkernel=%d) ==\n", n, reached_libkernel);
    return 2;
}
