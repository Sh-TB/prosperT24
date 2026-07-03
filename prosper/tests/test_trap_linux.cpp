// test_trap_linux — agentic-first M2 test (Linux only).
// Maps the real eboot into executable host memory, binds imports to a PROT_NONE
// stub region, and verifies that "calling" an import faults and is identified by
// name. Proves the map + relocate + trap-and-identify chain end to end.
#include "../src/self/module.hpp"
#include "../src/host/exec_image.hpp"
#include <cstdio>
#include <cstring>
#include <string>

using namespace prosper;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; \
    printf("  [FAIL] %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "../../PPSA24651-app0/eboot.bin";
    printf("== test_trap_linux: %s ==\n", path);

    std::string err;
    auto mo = Module::load(path, &err);
    CHECK(mo.has_value(), "load failed: %s", err.c_str());
    if (!mo) return 1;
    Module& m = *mo;

    const uint64_t BASE = 0x400000000ull;     // 16 GiB — clear of the host process
    const uint64_t STUB_BASE = 0x500000000ull; // +4 GiB, clear of the image
    const uint64_t STUB_SZ = 16;

    LoadedImage img = build_image(m, BASE);
    bind_imports_to_stubs(m, img, STUB_BASE, STUB_SZ);
    size_t applied = apply_relocations(m, img);
    CHECK(applied > 50000, "too few relocs applied: %zu", applied);

    if (!map_image(m, img, STUB_BASE, STUB_SZ, &err)) {
        printf("  [FAIL] map_image: %s\n", err.c_str());
        printf("\n== %d passed, %d failed ==\n", g_pass, g_fail + 1);
        return g_fail + 1;
    }
    install_trap_handler();

    // The image is now resident & executable at BASE; the entry byte must be
    // real code (fetched through the live mapping, not the staging vector).
    const uint8_t* entry = (const uint8_t*)img.entry;
    bool nz = false; for (int i = 0; i < 16; i++) if (entry[i]) nz = true;
    CHECK(nz, "entry code not present in live mapping");

    // Find representative imports and verify trap-by-name.
    auto find = [&](const char* lib) -> long {
        for (size_t i = 0; i < m.imports.size(); i++) if (m.imports[i].lib_name == lib) return (long)i;
        return -1;
    };
    struct { const char* lib; long idx; } probes[] = {
        {"libkernel", find("libkernel")},
        {"libSceAgc", find("libSceAgc")},
        {"libc",      find("libc")},
        {"libSceVideoOut", find("libSceVideoOut")},
    };
    for (auto& p : probes) {
        CHECK(p.idx >= 0, "no import found for %s", p.lib);
        if (p.idx < 0) continue;
        std::string name;
        int kind = invoke_stub((uint64_t)p.idx, &name);
        CHECK(kind == 1, "%s: expected stub trap (1), got %d", p.lib, kind);
        CHECK(name.rfind(std::string(p.lib) + "::", 0) == 0,
              "%s: trap name '%s' does not start with '%s::'", p.lib, name.c_str(), p.lib);
        printf("  trapped import[%ld] -> %s\n", p.idx, name.c_str());
    }

    printf("\n== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail;
}
