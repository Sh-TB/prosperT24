// exec_image_linux.cpp — Linux host backing + import trap (M2). Compiles to
// nothing on non-Linux hosts so the shared build (mingw) is unaffected.
#include "exec_image.hpp"
#include "../hle/nid.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace prosper {

namespace {
    const Module* g_mod = nullptr;
    uint64_t g_stub_base = 0, g_stub_size = 0, g_nstubs = 0;
    sigjmp_buf g_jb;
    // Signal handler is kept async-signal-safe: it only stores indices/addresses.
    // All name resolution (which touches std::string / maps) happens after longjmp.
    volatile sig_atomic_t g_trap_kind = 0;
    volatile long         g_trap_index = -1;   // import index if kind==1
    void*    g_fault_addr = nullptr;
    uint64_t g_fault_rip = 0;
    NidDb*   g_nid_db = nullptr;

    void segv_handler(int, siginfo_t* si, void* uctx) {
        uint64_t a = (uint64_t)si->si_addr;
        g_fault_addr = si->si_addr;
        auto* uc = (ucontext_t*)uctx;
        g_fault_rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
        if (a >= g_stub_base && a < g_stub_base + g_nstubs * g_stub_size) {
            g_trap_index = (long)((a - g_stub_base) / g_stub_size);
            g_trap_kind = 1;
        } else {
            g_trap_index = -1;
            g_trap_kind = 2;
        }
        siglongjmp(g_jb, 1);
    }

    // Build a human-readable description of the last trap. Safe (post-longjmp) context.
    std::string trap_detail() {
        char buf[320];
        if (g_trap_kind == 1 && g_mod && g_trap_index >= 0 &&
            (size_t)g_trap_index < g_mod->imports.size()) {
            const auto& im = g_mod->imports[g_trap_index];
            std::string name = g_nid_db ? g_nid_db->resolve(im.nid) : std::string();
            if (!name.empty())
                snprintf(buf, sizeof buf, "%s::%s  [%s]", im.lib_name.c_str(), im.nid.c_str(), name.c_str());
            else
                snprintf(buf, sizeof buf, "%s::%s", im.lib_name.c_str(), im.nid.c_str());
        } else {
            snprintf(buf, sizeof buf, "<non-stub fault at %p, rip=0x%llx>",
                     g_fault_addr, (unsigned long long)g_fault_rip);
        }
        return buf;
    }

    uint64_t page_up(uint64_t v) { return (v + 0xfff) & ~((uint64_t)0xfff); }
}

bool map_image(const Module& m, const LoadedImage& img,
               uint64_t stub_base, uint64_t stub_size, std::string* err) {
    auto fail = [&](const char* s){ if (err) *err = s; return false; };

    // Image: reserve at guest base and copy the relocated bytes in.
    // (M2a maps RWX for simplicity; per-segment W^X protection is a later refinement
    // because ELF LOAD segments here share pages and need careful splitting.)
    void* want = (void*)(img.base + img.min_vaddr);
    size_t sz  = img.mem.size();
    void* got = mmap(want, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (got == MAP_FAILED) return fail("mmap image failed");
    if (got != want) return fail("image base address unavailable");
    memcpy(got, img.mem.data(), sz);

    // Stub region: PROT_NONE so any guest call to an import faults.
    uint64_t nstubs = m.imports.size();
    uint64_t region = page_up(nstubs * stub_size);
    void* sw = (void*)stub_base;
    void* sg = mmap(sw, region, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (sg == MAP_FAILED || sg != sw) return fail("mmap stub region failed");

    g_mod = &m; g_stub_base = stub_base; g_stub_size = stub_size; g_nstubs = nstubs;
    return true;
}

void install_trap_handler() {
    if (!g_nid_db) g_nid_db = new NidDb();     // seed the readable-name dictionary once
    struct sigaction sa{};
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
}

int invoke_stub(uint64_t idx, std::string* name_out) {
    g_trap_kind = 0; g_trap_index = -1;
    if (sigsetjmp(g_jb, 1) == 0) {
        auto fn = (void(*)())(g_stub_base + idx * g_stub_size);
        fn();                    // jumps into PROT_NONE stub -> faults -> handler
        return 0;                // should never reach here
    }
    if (name_out) *name_out = trap_detail();
    return (int)g_trap_kind;
}

BootResult run_entry(const LoadedImage& img) {
    // A guest stack (16 MiB), plus a SysV-style block at the top: [argc][argv..][0][envp 0][auxv AT_NULL].
    const size_t STK = 16 * 1024 * 1024;
    void* stk = mmap(nullptr, STK, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    BootResult r;
    if (stk == MAP_FAILED) { r.kind = 2; r.detail = "guest stack mmap failed"; return r; }

    // Build the initial stack contents near the top (16-byte aligned).
    uint64_t top = (uint64_t)stk + STK;
    top &= ~(uint64_t)0xf;
    // place an "eboot" arg string
    static const char argstr[] = "/app0/eboot.bin";
    top -= sizeof(argstr); uint64_t arg0 = top;
    memcpy((void*)arg0, argstr, sizeof(argstr));
    top &= ~(uint64_t)0xf;
    // stack vector: argc, argv0, NULL, envp NULL, auxv(AT_NULL=0,0)
    uint64_t vec[] = { 1, arg0, 0, 0, 0, 0 };
    top -= sizeof(vec);
    top &= ~(uint64_t)0xf;            // ensure %rsp % 16 == 0 at entry
    memcpy((void*)top, vec, sizeof(vec));
    uint64_t sp = top;
    uint64_t rdi = sp;               // Sony crt: rdi -> {argc, argv...}
    uint64_t rsi = 0;

    g_trap_kind = 0; g_trap_index = -1; g_fault_addr = nullptr; g_fault_rip = 0;
    if (sigsetjmp(g_jb, 1) == 0) {
        register uint64_t e  asm("rax") = img.entry;
        register uint64_t s  asm("r8")  = sp;
        register uint64_t d  asm("r9")  = rdi;
        register uint64_t si asm("r10") = rsi;
        __asm__ volatile(
            "mov %%r8, %%rsp\n\t"
            "mov %%r9, %%rdi\n\t"
            "mov %%r10, %%rsi\n\t"
            "xor %%rbp, %%rbp\n\t"
            "jmp *%%rax\n\t"
            : : "r"(e), "r"(s), "r"(d), "r"(si) : "memory");
        r.kind = 0; r.detail = "entry returned without faulting"; // unreachable normally
    } else {
        r.kind = (int)g_trap_kind;
        r.detail = trap_detail();
        r.fault_addr = (uint64_t)g_fault_addr;
        r.fault_rip = g_fault_rip;
    }
    return r;
}

} // namespace prosper
#endif // __linux__
