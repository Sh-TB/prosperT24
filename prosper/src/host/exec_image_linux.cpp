// exec_image_linux.cpp — Linux host backing + HLE stubs (M2/M3). Compiles to nothing
// on non-Linux so the shared (mingw) build is unaffected.
#include "exec_image.hpp"
#include "../hle/nid.hpp"
#include "../hle/dispatch.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace prosper {

namespace {
    uint64_t g_base = 0, g_stub_base = 0, g_stub_size = 0, g_nstubs = 0;
    // Per-thread recovery point: only the thread that armed it can be longjmp'd back
    // (siglongjmp across threads is undefined). Guest worker threads that fault have no
    // armed point, so we terminate the process cleanly instead of corrupting state.
    thread_local sigjmp_buf g_jb;
    thread_local bool g_armed = false;
    volatile sig_atomic_t g_trap_kind = 0;   // 0 none, 2 SEGV/BUS, 3 ILL
    volatile int          g_trap_sig = 0;
    void*    g_fault_addr = nullptr;
    uint64_t g_fault_rip = 0;
    uint64_t g_rbp = 0, g_rsp = 0, g_rax = 0, g_rdi = 0, g_rsi = 0, g_rdx = 0;
    NidDb*   g_nid_db = nullptr;

    void fault_handler(int sig, siginfo_t* si, void* uctx) {
        g_fault_addr = si->si_addr;
        auto* uc = (ucontext_t*)uctx;
        auto& g = uc->uc_mcontext.gregs;
        g_fault_rip = (uint64_t)g[REG_RIP];
        g_rbp = (uint64_t)g[REG_RBP]; g_rsp = (uint64_t)g[REG_RSP];
        g_rax = (uint64_t)g[REG_RAX]; g_rdi = (uint64_t)g[REG_RDI];
        g_rsi = (uint64_t)g[REG_RSI]; g_rdx = (uint64_t)g[REG_RDX];
        g_trap_sig = sig;
        g_trap_kind = (sig == SIGILL) ? 3 : 2;
        if (g_armed) siglongjmp(g_jb, 1);
        // Fault on a thread with no recovery point (e.g. a guest worker thread during
        // bring-up): terminate cleanly rather than longjmp across threads.
        _exit(90);
    }

    std::string trap_detail() {
        char buf[256];
        const char* sn = g_trap_sig == SIGILL ? "SIGILL" : g_trap_sig == SIGBUS ? "SIGBUS" : "SIGSEGV";
        uint64_t off = (g_base && g_fault_rip >= g_base) ? g_fault_rip - g_base : 0;
        snprintf(buf, sizeof buf, "%s at addr=%p  rip=0x%llx (image+0x%llx)",
                 sn, g_fault_addr, (unsigned long long)g_fault_rip, (unsigned long long)off);
        return buf;
    }

    uint64_t page_up(uint64_t v) { return (v + 0xfff) & ~((uint64_t)0xfff); }

    // Emit machine code into a stub slot.
    void emit_impl(uint8_t* p, uint64_t fn) {          // movabs rax,fn ; jmp rax
        p[0] = 0x48; p[1] = 0xB8; memcpy(p + 2, &fn, 8); p[10] = 0xFF; p[11] = 0xE0;
    }
    void emit_unimpl(uint8_t* p, uint32_t idx, uint64_t fn) { // mov edi,idx ; movabs rax,fn ; jmp rax
        p[0] = 0xBF; memcpy(p + 1, &idx, 4);
        p[5] = 0x48; p[6] = 0xB8; memcpy(p + 7, &fn, 8); p[15] = 0xFF; p[16] = 0xE0;
    }
}

bool map_image(const LoadedImage& img, std::string* err) {
    auto fail = [&](const char* s){ if (err) *err = s; return false; };
    void* want = (void*)(img.base + img.min_vaddr);
    size_t sz  = img.mem.size();
    // RWX for bring-up; per-segment W^X is a later refinement (shared LOAD pages).
    void* got = mmap(want, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (got == MAP_FAILED || got != want) return fail("mmap image at guest base failed");
    memcpy(got, img.mem.data(), sz);
    if (!g_base) g_base = img.base;   // main image, for fault-offset reporting
    return true;
}

bool install_stubs(const std::vector<ImportSlot>& slots, uint64_t stub_base,
                   uint64_t stub_size, std::string* err) {
    auto fail = [&](const char* s){ if (err) *err = s; return false; };
    if (stub_size < 24) return fail("stub_size too small (need >= 24)");
    if (!g_nid_db) g_nid_db = new NidDb();
    dispatch_init(&slots, g_nid_db);

    uint64_t n = slots.size();
    uint64_t region = page_up(n * stub_size);
    void* want = (void*)stub_base;
    void* got = mmap(want, region, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (got == MAP_FAILED || got != want) return fail("mmap stub region failed");

    uint8_t* base = (uint8_t*)got;
    for (uint64_t i = 0; i < n; i++) {
        uint8_t* slot = base + i * stub_size;
        HleFn fn = Hle::lookup(slots[i].nid);
        if (fn) emit_impl(slot, (uint64_t)fn);
        else    emit_unimpl(slot, (uint32_t)i, (uint64_t)&prosper_on_unimpl);
    }
    g_stub_base = stub_base; g_stub_size = stub_size; g_nstubs = n;
    return true;
}

void install_trap_handler() {
    struct sigaction sa{};
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
}

uint64_t stub_addr(uint64_t idx) { return g_stub_base + idx * g_stub_size; }

uint64_t invoke_stub(uint64_t idx) {
    auto fn = (uint64_t(*)())(stub_addr(idx));
    return fn();
}

size_t run_guest_inits(const std::vector<uint64_t>& fns) {
    size_t ok = 0;
    for (uint64_t f : fns) {
        g_trap_kind = 0; g_armed = true;
        if (sigsetjmp(g_jb, 1) == 0) { ((void (*)())(uintptr_t)f)(); ok++; }
        g_armed = false;
        if (g_trap_kind) fprintf(stderr, "[prosper] init fn 0x%llx faulted (%s); continuing\n",
                                 (unsigned long long)f, trap_detail().c_str());
    }
    return ok;
}

BootResult run_entry(const LoadedImage& img) {
    const size_t STK = 16 * 1024 * 1024;
    void* stk = mmap(nullptr, STK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    BootResult r;
    if (stk == MAP_FAILED) { r.kind = 2; r.detail = "guest stack mmap failed"; return r; }

    uint64_t top = ((uint64_t)stk + STK) & ~(uint64_t)0xf;
    static const char argstr[] = "/app0/eboot.bin";
    top -= sizeof(argstr); uint64_t arg0 = top;
    memcpy((void*)arg0, argstr, sizeof(argstr));
    top &= ~(uint64_t)0xf;
    uint64_t vec[] = { 1, arg0, 0, 0, 0, 0 };   // argc, argv0, NULL, envp NULL, auxv AT_NULL
    top -= sizeof(vec); top &= ~(uint64_t)0xf;   // 16-aligned base for the vector
    // The Sony crt _start pushes an odd number of words before its first call, so it
    // expects entry rsp ≡ 8 (mod 16) (like a normal callee), NOT 16-aligned. Placing
    // the vector 8 below a 16-boundary makes every downstream call correctly aligned,
    // so alignment-sensitive SIMD (vmovaps) in callees doesn't #GP.
    top -= 8;
    memcpy((void*)top, vec, sizeof(vec));
    uint64_t sp = top, rdi = sp, rsi = 0;

    g_trap_kind = 0; g_fault_addr = nullptr; g_fault_rip = 0; g_armed = true;
    if (sigsetjmp(g_jb, 1) == 0) {
        register uint64_t e  asm("rax") = img.entry;
        register uint64_t s  asm("r8")  = sp;
        register uint64_t d  asm("r9")  = rdi;
        register uint64_t si asm("r10") = rsi;
        __asm__ volatile(
            "mov %%r8, %%rsp\n\t" "mov %%r9, %%rdi\n\t" "mov %%r10, %%rsi\n\t"
            "xor %%rbp, %%rbp\n\t" "jmp *%%rax\n\t"
            : : "r"(e), "r"(s), "r"(d), "r"(si) : "memory");
        r.kind = 0; r.detail = "entry returned";
    } else {
        r.kind = (int)g_trap_kind;
        r.detail = trap_detail();
        r.fault_addr = (uint64_t)g_fault_addr;
        r.fault_rip = g_fault_rip;
        r.rbp = g_rbp; r.rsp = g_rsp; r.rax = g_rax;
        r.rdi = g_rdi; r.rsi = g_rsi; r.rdx = g_rdx;
        // Walk the rbp chain for a backtrace, guarded so a bad frame can't crash us.
        g_armed = true;
        if (sigsetjmp(g_jb, 1) == 0) {
            uint64_t bp = g_rbp;
            for (int i = 0; i < 24 && bp > 0x10000; i++) {
                uint64_t ret = *(uint64_t*)(bp + 8);
                if (ret) r.backtrace.push_back(ret);
                uint64_t nbp = *(uint64_t*)bp;
                if (nbp <= bp) break;
                bp = nbp;
            }
        }
        g_armed = false;
    }
    return r;
}

} // namespace prosper
#endif // __linux__
