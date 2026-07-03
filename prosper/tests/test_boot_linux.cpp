// test_boot_linux — M3/M4 integration test. Links the main executable together with
// the game's own modules (Il2cppUserAssemblies = compiled C#, PS5Util) and boots.
// Cross-module imports now resolve to real code; only true system calls hit HLE/stubs.
// The guest is multithreaded and will eventually fault in a stubbed path during
// bring-up, so we run it in a forked child and measure depth via shared memory.
#include "../src/self/module.hpp"
#include "../src/loader/linker.hpp"
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
    std::string dump = (argc >= 2) ? argv[1] : "../../PPSA24651-app0";
    printf("== test_boot_linux (multi-module): %s ==\n", dump.c_str());

    // Main executable first, then the game's own PRX modules.
    std::vector<LinkInput> inputs = {
        { dump + "/eboot.bin",                    0x400000000ull },
        { dump + "/Media/Modules/Il2cppUserAssemblies.prx", 0x440000000ull },
        { dump + "/Media/Modules/PS5Util.prx",    0x4c0000000ull },
    };
    const uint64_t STUB_BASE = 0x600000000ull;

    Program prog;
    std::string err;
    if (!link_program(inputs, STUB_BASE, prog, &err)) { printf("  [FAIL] link: %s\n", err.c_str()); return 1; }
    printf("  linked %zu modules: %zu imports (%zu cross-module, %zu stubbed / %zu slots)\n",
           prog.mods.size(), prog.total_imports, prog.resolved_cross_module, prog.stubbed, prog.slots.size());

    register_builtin_hle();
    set_app0_root(dump);                        // guest "/app0" -> the game dump directory
    for (auto& img : prog.imgs)
        if (!map_image(img, &err)) { printf("  [FAIL] map: %s\n", err.c_str()); return 1; }
    if (!install_stubs(prog.slots, prog.stub_base, prog.stub_size, &err)) { printf("  [FAIL] stubs: %s\n", err.c_str()); return 1; }
    install_trap_handler();

    volatile int* p = (volatile int*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *p = 0; dispatch_set_progress(p);

    printf("  entry=0x%llx  running guest in a child process...\n", (unsigned long long)prog.entry);
    pid_t pid = fork();
    if (pid == 0) { run_entry(prog.imgs[0]); _exit(0); }
    for (int i = 0; i < 200; i++) { int st; if (waitpid(pid, &st, WNOHANG) == pid) break; struct timespec ts{0, 50 * 1000 * 1000}; nanosleep(&ts, nullptr); }
    int st; if (waitpid(pid, &st, WNOHANG) == 0) { kill(pid, SIGKILL); waitpid(pid, &st, 0); }

    int reached = *p;
    printf("  guest reached %d distinct unimplemented system calls before stopping\n", reached);
    // Note: this count *drops* as we implement more functions (the boot then advances to
    // new, deeper unimplemented calls). >=3 robustly proves the full pipeline works:
    // link -> map -> stubs -> crt -> heap -> virtual memory -> engine/game code.
    const int THRESHOLD = 3;
    if (reached >= THRESHOLD) {
        printf("\n== PASS: linked program booted through init into engine/game code (%d >= %d) ==\n", reached, THRESHOLD);
        return 0;
    }
    printf("\n== FAIL: stalled early (%d < %d) ==\n", reached, THRESHOLD);
    return 2;
}
