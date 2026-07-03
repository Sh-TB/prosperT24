// test_trap_linux — M2/M3: map the real eboot executable, install HLE stubs, and
// verify that "calling" an import routes through dispatch: the unimplemented logger
// records it (by index -> lib::nid) and returns 0. Proves map + relocate + stub +
// dispatch end to end.
#include "../src/self/module.hpp"
#include "../src/host/exec_image.hpp"
#include "../src/hle/dispatch.hpp"
#include <cstdio>
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
    CHECK(mo.has_value(), "load: %s", err.c_str());
    if (!mo) return 1;
    Module& m = *mo;

    const uint64_t BASE = 0x400000000ull, STUB_BASE = 0x500000000ull, STUB_SZ = 32;
    LoadedImage img = build_image(m, BASE);
    bind_imports_to_stubs(m, img, STUB_BASE, STUB_SZ);
    CHECK(apply_relocations(m, img) > 50000, "too few relocs applied");
    CHECK(map_image(m, img, &err), "map_image: %s", err.c_str());
    CHECK(install_stubs(m, STUB_BASE, STUB_SZ, &err), "install_stubs: %s", err.c_str());
    install_trap_handler();

    auto find = [&](const char* lib) -> long {
        for (size_t i = 0; i < m.imports.size(); i++) if (m.imports[i].lib_name == lib) return (long)i;
        return -1;
    };
    for (const char* lib : {"libkernel", "libSceAgc", "libc", "libSceVideoOut"}) {
        long idx = find(lib);
        CHECK(idx >= 0, "no import for %s", lib);
        if (idx < 0) continue;
        reset_call_log();
        uint64_t ret = invoke_stub((uint64_t)idx);           // acts as a guest call
        CHECK(ret == 0, "%s stub should return 0 (unimplemented), got %llu", lib, (unsigned long long)ret);
        CHECK(call_order().size() == 1 && call_order()[0] == (uint32_t)idx,
              "%s: dispatch did not record import #%ld", lib, idx);
        printf("  dispatched import[%ld] %s::%s -> 0\n", idx, lib, m.imports[idx].nid.c_str());
    }

    printf("\n== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail;
}
