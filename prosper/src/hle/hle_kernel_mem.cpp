// hle_kernel_mem.cpp — HLE of libkernel virtual/direct memory (Linux backing).
// PS5 memory model: reserve a virtual range, allocate "direct" (physical) memory as
// an opaque offset, then map it (or flexible memory) into VA. We back all of it with
// host mmap and hand the guest real addresses through the in/out pointer args.
// Guarded to Linux; on other hosts the registrations are no-ops.
#include "dispatch.hpp"
#include "nid.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <cstdint>
#include <atomic>

namespace prosper {

#define HLE(name) static uint64_t name(uint64_t a0, uint64_t a1, uint64_t a2, \
                                       uint64_t a3, uint64_t a4, uint64_t a5)

namespace {
    // "Direct memory" is modelled as a monotonically growing offset space.
    std::atomic<uint64_t> g_dmem_off{0x10000000};   // start at 256 MiB to look plausible
    // Sony PROT bits: 1=CPU_READ 2=CPU_WRITE 4=CPU_EXEC (+GPU bits we fold into RW).
    int host_prot(uint64_t p) {
        int hp = 0;
        if (p & 0x1) hp |= PROT_READ;
        if (p & 0x2) hp |= PROT_READ | PROT_WRITE;
        if (p & 0x4) hp |= PROT_EXEC;
        if (!hp) hp = PROT_READ | PROT_WRITE;        // default RW so bring-up doesn't perm-fault
        return hp;
    }
    void* map_at(uint64_t hint, uint64_t len, int prot) {
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
        if (hint) flags |= MAP_FIXED;                // overlay a prior PROT_NONE reservation
        void* p = mmap((void*)hint, len, prot, flags, -1, 0);
        return p == MAP_FAILED ? nullptr : p;
    }
}

// sceKernelReserveVirtualRange(void** addrInOut, size_t len, int flags, size_t align)
HLE(k_reserve_vrange) {
    uint64_t hint = a0 ? *(uint64_t*)a0 : 0;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | (hint ? MAP_FIXED_NOREPLACE : 0);
    void* p = mmap((void*)hint, a1, PROT_NONE, flags, -1, 0);
    if (p == MAP_FAILED) return 0x16;               // SCE_KERNEL_ERROR_EINVAL-ish
    if (a0) *(uint64_t*)a0 = (uint64_t)p;
    return 0;
}

// sceKernelMapNamedFlexibleMemory(void** addrInOut, size_t len, int prot, int flags, const char* name)
HLE(k_map_flexible) {
    uint64_t hint = a0 ? *(uint64_t*)a0 : 0;
    void* p = map_at(hint, a1, host_prot(a2));
    if (!p) return 0x16;
    if (a0) *(uint64_t*)a0 = (uint64_t)p;
    return 0;
}

// sceKernelAllocateDirectMemory(off_t searchStart, off_t searchEnd, size_t len,
//                               size_t align, int memType, off_t* physAddrOut)
HLE(k_alloc_dmem) {
    uint64_t align = a3 ? a3 : 0x4000;
    uint64_t off = (g_dmem_off += ((a2 + align - 1) & ~(align - 1))) - ((a2 + align - 1) & ~(align - 1));
    off = (off + align - 1) & ~(align - 1);
    if (a5) *(uint64_t*)a5 = off;
    return 0;
}

// sceKernelMapDirectMemory(void** addrInOut, size_t len, int prot, int flags,
//                          off_t physAddr, size_t align)
HLE(k_map_dmem) {
    uint64_t hint = a0 ? *(uint64_t*)a0 : 0;
    void* p = map_at(hint, a1, host_prot(a2));
    if (!p) return 0x16;
    if (a0) *(uint64_t*)a0 = (uint64_t)p;
    return 0;
}

// sceKernelVirtualQuery(const void* addr, int flags, SceKernelVirtualQueryInfo* info, size_t infoSize)
// Fill the info struct so the guest doesn't consume uninitialized memory. Layout:
//   0x00 void* start; 0x08 void* end; 0x10 off_t offset; 0x18 i32 protection;
//   0x1C i32 memoryType; 0x20 u32 flags(bits); 0x24 char name[32].
HLE(k_virtual_query) {
    if (!a2) return 0x16;
    uint8_t* info = (uint8_t*)a2;
    uint64_t sz = a3 ? a3 : 0x48;
    if (sz > 0x48) sz = 0x48;
    for (uint64_t i = 0; i < sz; i++) info[i] = 0;
    uint64_t start = a0 & ~(uint64_t)0x3fff;
    if (sz >= 0x08) *(uint64_t*)(info + 0x00) = start;
    if (sz >= 0x10) *(uint64_t*)(info + 0x08) = start + 0x40000000ull; // 1 GiB region
    if (sz >= 0x1c) *(int32_t*)(info + 0x18) = 0x3;                    // RW
    if (sz >= 0x24) *(uint32_t*)(info + 0x20) = 0x10;                  // "committed"-ish
    return 0;
}

HLE(k_munmap)      { if (a0) munmap((void*)a0, a1); return 0; }
HLE(k_mprotect)    { if (a0) mprotect((void*)a0, a1, host_prot(a2)); return 0; }
// Direct-memory sizing queries — report a big pool so allocators are satisfied.
HLE(k_dmem_size)   { return 8ull * 1024 * 1024 * 1024; }   // 8 GiB

void register_kernel_mem_hle() {
    #define R(str, fn) Hle::register_fn(nid_hash(str), (HleFn)(fn), str)
    R("sceKernelReserveVirtualRange", k_reserve_vrange);
    R("sceKernelMapNamedFlexibleMemory", k_map_flexible);
    R("sceKernelMapFlexibleMemory", k_map_flexible);
    R("sceKernelAllocateDirectMemory", k_alloc_dmem);
    R("sceKernelAllocateMainDirectMemory", k_alloc_dmem);
    R("sceKernelMapDirectMemory", k_map_dmem);
    R("sceKernelMapNamedDirectMemory", k_map_dmem);
    R("sceKernelMunmap", k_munmap);
    R("sceKernelMprotect", k_mprotect);
    R("sceKernelVirtualQuery", k_virtual_query);
    R("sceKernelGetDirectMemorySize", k_dmem_size);
    R("sceKernelAvailableDirectMemorySize", k_dmem_size);
    #undef R
}

} // namespace prosper

#else  // non-Linux: memory HLE not available yet
namespace prosper { void register_kernel_mem_hle() {} }
#endif
