// test_boot_linux — M3 integration test. Boots the real guest with the HLE layer and
// asserts it executes deep into engine init. The guest is now multithreaded and will,
// during bring-up, eventually fault/exit in a stubbed path — so we run it in a forked
// child and measure how far it got via a shared-memory progress counter that survives
// the child's death. "Deep progress" (many distinct Sony calls incl. threading) = pass.
#include "../src/self/module.hpp"
#include "../src/host/exec_image.hpp"
#include "../src/hle/dispatch.hpp"
#include <cstdio>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctime>

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
    if (!map_image(m, img, &err))                   { printf("  [FAIL] map: %s\n", err.c_str()); return 1; }
    register_builtin_hle();
    if (!install_stubs(m, STUB_BASE, STUB_SZ, &err)) { printf("  [FAIL] stubs: %s\n", err.c_str()); return 1; }
    install_trap_handler();

    // Shared progress counter (survives fork + child crash).
    volatile int* prog = (volatile int*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *prog = 0;
    dispatch_set_progress(prog);

    printf("  entry=0x%llx  running guest in a child process...\n", (unsigned long long)img.entry);
    pid_t pid = fork();
    if (pid == 0) {                    // child: run the guest (may crash/abort/_exit)
        run_entry(img);
        _exit(0);
    }
    // Parent: bound the run, then read how far the child got.
    for (int i = 0; i < 200; i++) {    // up to ~10s
        int st; pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) break;
        struct timespec ts{0, 50 * 1000 * 1000}; nanosleep(&ts, nullptr);
    }
    int st; if (waitpid(pid, &st, WNOHANG) == 0) { kill(pid, SIGKILL); waitpid(pid, &st, 0); }

    int reached = *prog;
    printf("  guest reached %d distinct unimplemented Sony calls before stopping\n", reached);

    // Deep progress = crt + C++ init + heap + virtual memory + threading + game calls.
    const int THRESHOLD = 8;
    if (reached >= THRESHOLD) {
        printf("\n== PASS: guest booted deep into engine init (%d >= %d distinct calls) ==\n", reached, THRESHOLD);
        return 0;
    }
    printf("\n== FAIL: guest stalled early (%d < %d) ==\n", reached, THRESHOLD);
    return 2;
}
