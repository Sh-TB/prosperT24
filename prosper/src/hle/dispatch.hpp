// dispatch.hpp — HLE call dispatch: registry of implemented Sony functions +
// a logger for unimplemented ones. Because the Linux host ABI == the guest's
// System V AMD64 ABI, an HLE handler is just a C function the guest calls directly
// (args already in rdi/rsi/rdx/rcx/r8/r9); it returns a value in rax and rets to guest.
#pragma once
#include "../self/module.hpp"
#include "nid.hpp"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace prosper {

// Generic HLE handler signature (up to 6 integer/pointer args, SysV).
using HleFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// One unresolved import across the whole linked program (deduped by NID). Its index
// is the stub slot number; the trap logger names calls via this table.
struct ImportSlot { std::string lib, nid; };

// Registry of implemented functions, keyed by NID.
class Hle {
public:
    static void  register_fn(const std::string& nid, HleFn fn, const char* name);
    static HleFn lookup(const std::string& nid);          // nullptr if unimplemented
    static const char* name_of(const std::string& nid);   // registered display name or ""
};

// Wire the unimplemented-call logger to the global stub-slot table + name DB.
void dispatch_init(const std::vector<ImportSlot>* slots, NidDb* db);

// Register the built-in HLE implementations (libc thunks, CRT no-ops, libkernel
// primitives). Call before install_stubs.
void register_builtin_hle();
// libkernel primitives (pthread/sync/etc.); called by register_builtin_hle().
void register_kernel_hle();
// File I/O (stdio + POSIX fd, with /app0 path translation); called by register_builtin_hle().
void register_file_hle();
// Set the host directory backing the guest's "/app0" (the game data root).
void set_app0_root(const std::string& root);
// PS5 system services (user/NP/pad/mouse/appcontent/dialog); called by register_builtin_hle().
void register_service_hle();
// Headless graphics bring-up (libSceAgc/libSceVideoOut placeholders); see hle_graphics.cpp.
void register_graphics_hle();
// libkernel virtual/direct memory (Linux backing); called by register_kernel_hle().
void register_kernel_mem_hle();
// libkernel time/clock + C11 threads + assorted stubs; called by register_kernel_hle().
void register_kernel_time_hle();

// The default target for unimplemented imports: logs (first-seen) and returns 0.
// Called by generated stubs with the import index in the first arg.
extern "C" uint64_t prosper_on_unimpl(uint64_t import_index);

// First-seen order of unimplemented import indices called by the guest.
const std::vector<uint32_t>& call_order();
// Optional external progress counter (e.g. shared memory), incremented once per
// first-seen unimplemented call. Survives across fork() so a crash-prone deep boot can
// still be measured by a parent process.
void dispatch_set_progress(volatile int* counter);
// Optional external counter incremented on each sceKernelRaiseException delivery (the GC's
// stop-the-world). Also fork-safe. Used by tests to prove the boot reached — and got through —
// the IL2CPP GC thread-suspension handshake (a regression guard for the deadlock/GC fixes).
void set_exc_raise_counter(volatile int* counter);
// Optional fork-safe counter bumped when the guest calls into the graphics libs (libSceAgc /
// libSceVideoOut). Tests use it to prove the boot advanced through the whole runtime into GPU/
// display init. Defined in hle_graphics.cpp.
void set_gfx_call_counter(volatile int* counter);
// Per-module TLS template for the general-dynamic model (see linker.cpp / __tls_get_addr).
// init_va = mapped tdata (guest==host addr); a per-thread block of memsz is allocated lazily,
// filesz bytes copied from init_va, the rest zeroed. Indexed by module TLS id.
struct TlsModuleDesc { uint64_t init_va = 0, filesz = 0, memsz = 0; };
// Install the TLS templates (call AFTER images are mapped, so init_va is readable). Enables the
// __tls_get_addr HLE to serve real per-thread TLS blocks for loaded modules (e.g. real libc.prx).
void set_tls_modules(const TlsModuleDesc* descs, size_t count);

// Guest address of the main module's SCE_PROCPARAM segment. sceKernelGetProcParam returns this;
// real libc reads its heap/malloc config (sceLibcParam) from it, so a correct value is required for
// real libc.prx's heap to initialize. Set from the eboot's PT_SCE_PROCPARAM segment after mapping.
void set_proc_param(uint64_t guest_va);

// Print the accumulated unimplemented-call trace (index, lib::nid [name], count).
void dump_call_log(FILE* f);
void reset_call_log();

} // namespace prosper
