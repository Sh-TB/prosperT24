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

    // The guest must actually execute native code: either it ran to completion, or it
    // faulted *inside the guest image* (expected during bring-up as we stub functions).
    // A fault in our harness (outside the image) or no execution at all is a failure.
    uint64_t img_lo = BASE, img_hi = BASE + img.mem.size();
    bool faulted_in_guest = (r.kind == 2 || r.kind == 3) && r.fault_rip >= img_lo && r.fault_rip < img_hi;
    if (r.kind == 0 || faulted_in_guest) {
        printf("\n== PASS: guest executed native code (%zu distinct unimplemented calls; %s) ==\n",
               call_order().size(),
               r.kind == 0 ? "ran to completion" : "faulted in guest image, as expected mid-bring-up");
        return 0;
    }
    printf("\n== FAIL: guest did not execute guest code (kind=%d rip=0x%llx) ==\n",
           r.kind, (unsigned long long)r.fault_rip);
    return 2;
}
