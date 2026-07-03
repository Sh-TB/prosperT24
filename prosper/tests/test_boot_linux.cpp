// test_boot_linux — M2b: jump into the real guest entry point and run until the
// first fault. Success = the crt executed and reached its first Sony import call
// (trap kind 1), which we identify by name. Fully headless/self-checking.
#include "../src/self/module.hpp"
#include "../src/host/exec_image.hpp"
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

    const uint64_t BASE = 0x400000000ull, STUB_BASE = 0x500000000ull, STUB_SZ = 16;
    LoadedImage img = build_image(m, BASE);
    bind_imports_to_stubs(m, img, STUB_BASE, STUB_SZ);
    apply_relocations(m, img);
    if (!map_image(m, img, STUB_BASE, STUB_SZ, &err)) { printf("  [FAIL] map: %s\n", err.c_str()); return 1; }
    install_trap_handler();

    printf("  entry = 0x%llx  jumping in...\n", (unsigned long long)img.entry);
    BootResult r = run_entry(img);

    printf("  -> kind=%d  detail='%s'\n", r.kind, r.detail.c_str());
    printf("     fault_addr=0x%llx  guest_rip=0x%llx (base+0x%llx)\n",
           (unsigned long long)r.fault_addr, (unsigned long long)r.fault_rip,
           (unsigned long long)(r.fault_rip - BASE));

    // The crt must have executed past its prologue and reached its first import.
    if (r.kind == 1) {
        printf("\n== PASS: guest executed and trapped at first Sony call: %s ==\n", r.detail.c_str());
        return 0;
    }
    printf("\n== FAIL: expected an import trap (kind 1); got kind %d ==\n", r.kind);
    return 2;
}
